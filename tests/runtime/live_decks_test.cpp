// Looks (Parsen, Schreiben, Herauslösen) und die Live-Decks mit ihrer Überblendung.
//
// Die Decks werden ohne GPU geprüft: Ob der Live-Graph stimmt, zeigt der Feldcompiler, der ihn
// übersetzen muss; wie weit die Überblendung ist, zeigt der Parameter `amount` des Mischers.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "builtin_lab.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/live_decks.hpp"
#include "whitehole/runtime/live_look.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

namespace {

const char* const kProjectWithLooks = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
end
output plasma.out

look "Ruhig" fade 3
node n1 wh.generator.plasma
  set scale 2
end
node n2 wh.transform.mirror
end
connect n1.out -> n2.source
output n2.out
endlook

look Schnell fade 0.5
node a wh.generator.plasma
end
output a.out
endlook
)";

std::string base(const std::string& lookBlock) {
    return std::string{"whitehole-project 1\ndestination 0 10.0.0.5\nfixture P\n  matrix 2 2\n  patch artnet destination 0 universe 1\nend\n"
                       "node p wh.generator.plasma\nend\noutput p.out\n"} +
           lookBlock;
}

project::Project parse(Lab& lab, const std::string& text) {
    auto parsed = project::parseProject(text, lab.registry);
    REQUIRE(parsed.ok());
    return std::move(parsed).value();
}

double amountOf(const runtime::LiveDecks& decks) {
    const graph::NodeInstance* mixer = decks.graph().findNode(decks.target().node);
    REQUIRE(mixer != nullptr);
    return std::get<double>(mixer->parameters.at("amount"));
}

}  // namespace

TEST_CASE("Looks: aus der Projektdatei lesen") {
    Lab lab;
    const project::Project project = parse(lab, kProjectWithLooks);
    REQUIRE(project.looks.size() == 2);
    CHECK(project.looks[0].name == "Ruhig");
    CHECK(project.looks[0].fadeSeconds == Approx(3.0));
    CHECK(project.looks[0].graph.nodes().size() == 2);
    CHECK(project.looks[0].graph.connections().size() == 1);
    CHECK(project.looks[0].graph.findNode(project.looks[0].target.node)->typeId == "wh.transform.mirror");
    CHECK(project.looks[1].name == "Schnell");
    CHECK(project.looks[1].fadeSeconds == Approx(0.5));
    // Der Hauptgraph bleibt davon unberührt
    CHECK(project.graph.nodes().size() == 1);
    REQUIRE(project.target.has_value());
}

TEST_CASE("Looks: Schreiben und wieder Lesen ergibt dasselbe") {
    Lab lab;
    const project::Project project = parse(lab, kProjectWithLooks);
    const auto text = project::writeProject(project);
    REQUIRE(text.ok());
    const project::Project again = parse(lab, text.value());
    REQUIRE(again.looks.size() == 2);
    CHECK(again.looks[0].name == "Ruhig");
    CHECK(again.looks[0].fadeSeconds == Approx(3.0));
    CHECK(again.looks[0].graph.nodes().size() == 2);
    CHECK(again.looks[1].graph.nodes().size() == 1);
    const auto second = project::writeProject(again);
    REQUIRE(second.ok());
    CHECK(second.value() == text.value());  // deterministisch
}

TEST_CASE("Looks: Fehler beim Lesen nennen die Zeile") {
    Lab lab;
    const auto fails = [&](const std::string& block, const char* part) {
        const auto parsed = project::parseProject(base(block), lab.registry);
        REQUIRE_FALSE(parsed.ok());
        CHECK(parsed.error().message.find(part) != std::string::npos);
    };
    fails("look A\nnode n wh.generator.plasma\nend\nendlook\n", "output");                 // kein Ausgang
    fails("look A\nnode n wh.generator.plasma\nend\noutput n.out\n", "endlook");           // nicht beendet
    fails("look A\nlook B\nendlook\n", "schachteln");
    fails("look A fade -1\nendlook\n", "fade");
    fails("look A fade 601\nendlook\n", "fade");
    fails("look A\nfixture X\nend\nendlook\n", "gehört nicht");
    fails("look A\nnode n wh.generator.plasma\nend\noutput n.out\nendlook\n"
          "look A\nnode n wh.generator.plasma\nend\noutput n.out\nendlook\n", "vergeben");  // Name doppelt
    fails("endlook\n", "ohne look");
}

TEST_CASE("Looks: captureLook nimmt nur auf, was den Ausgang speist") {
    Lab lab;
    project::Project project = parse(lab, std::string{kProjectWithLooks});
    const NodeId a = project.graph.addNode("wh.generator.noise", Vec2{}).value();
    const NodeId mirror = project.graph.addNode("wh.transform.mirror", Vec2{}).value();
    const NodeId lfo = project.graph.addNode("wh.value.lfo", Vec2{}).value();
    const NodeId stray = project.graph.addNode("wh.generator.plasma", Vec2{}).value();
    const NodeId scale = project.graph.addNode("wh.transform.scale", Vec2{}).value();
    REQUIRE(project.graph.connect({a, "out"}, {mirror, "source"}).ok());
    REQUIRE(project.graph.connect({mirror, "out"}, {scale, "source"}).ok());
    REQUIRE(project.graph.connect({lfo, "out"}, {scale, "factor"}).ok());
    (void)stray;

    const auto look = runtime::captureLook(project.graph, {scale, "out"}, "Test", 1.5);
    REQUIRE(look.ok());
    CHECK(look.value().graph.nodes().size() == 4);  // Noise, Mirror, LFO, Scale - der lose Plasma bleibt draußen
    CHECK(look.value().graph.connections().size() == 3);
    CHECK(look.value().graph.findNode(look.value().target.node)->typeId == "wh.transform.scale");
    CHECK(look.value().fadeSeconds == Approx(1.5));

    CHECK_FALSE(runtime::captureLook(project.graph, {scale, "out"}, "", 1.0).ok());
    CHECK_FALSE(runtime::captureLook(project.graph, {scale, "out"}, "x", -1.0).ok());
    CHECK_FALSE(runtime::captureLook(project.graph, {scale, "out"}, "x", 1000.0).ok());
    CHECK_FALSE(runtime::captureLook(project.graph, {NodeId{9999}, "out"}, "x", 1.0).ok());
    CHECK(runtime::isValidLookName("Sonnenaufgang 2"));
    CHECK_FALSE(runtime::isValidLookName("zwei\nZeilen"));
    CHECK(runtime::findLook(project.looks, "Schnell") == 1);
    CHECK(runtime::findLook(project.looks, "gibt es nicht") == -1);
}

TEST_CASE("Decks: Start zeigt den ersten Look, der Live-Graph lässt sich übersetzen") {
    Lab lab;
    const project::Project project = parse(lab, kProjectWithLooks);
    runtime::LiveDecks decks{lab.registry};
    REQUIRE(decks.start(project.looks[0], 0).ok());
    CHECK(decks.takeStructureChanged());
    CHECK_FALSE(decks.takeStructureChanged());  // wird beim Abfragen zurückgesetzt
    CHECK(decks.status().current == 0);
    CHECK(decks.status().armed == -1);
    CHECK(amountOf(decks) == Approx(0.0));
    CHECK(decks.graph().nodes().size() == 4);  // Plasma, Mirror, Mischer und der Platzhalter im leeren Deck
    const auto compiled = compiler::compileField(decks.graph(), lab.snippets, decks.target());
    INFO((compiled.ok() ? std::string{} : compiled.error().message));
    REQUIRE(compiled.ok());
}

TEST_CASE("Decks: arm, go und Überblendung über die Zeit") {
    Lab lab;
    const project::Project project = parse(lab, kProjectWithLooks);
    runtime::LiveDecks decks{lab.registry};
    REQUIRE(decks.start(project.looks[0], 0).ok());
    (void)decks.takeStructureChanged();

    REQUIRE(decks.arm(project.looks[1], 1).ok());
    CHECK(decks.takeStructureChanged());  // der Look kommt ins verdeckte Deck: neuer Shader
    CHECK(decks.status().armed == 1);
    CHECK(decks.status().current == 0);
    CHECK(amountOf(decks) == Approx(0.0));  // noch nichts zu sehen
    REQUIRE(compiler::compileField(decks.graph(), lab.snippets, decks.target()).ok());

    REQUIRE(decks.go(2.0).ok());
    CHECK(decks.status().fading);
    CHECK(decks.status().incoming == 1);
    CHECK(decks.advance(0.5));
    CHECK(amountOf(decks) == Approx(0.25));
    CHECK(decks.status().progress == Approx(0.25));
    CHECK(decks.advance(0.5));
    CHECK(amountOf(decks) == Approx(0.5));
    CHECK_FALSE(decks.takeStructureChanged());  // während des Fades ändern sich nur Werte
    CHECK(decks.advance(1.5));  // über das Ende hinaus: schließt ab
    CHECK_FALSE(decks.status().fading);
    CHECK(decks.status().current == 1);
    CHECK(amountOf(decks) == Approx(1.0));
    CHECK_FALSE(decks.advance(1.0));  // nichts mehr zu tun

    // Zurück zu Look 0: jetzt landet er im anderen Deck, und der Fade läuft von 1 nach 0
    REQUIRE(decks.arm(project.looks[0], 0).ok());
    REQUIRE(decks.go(1.0).ok());
    CHECK(decks.advance(0.25));
    CHECK(amountOf(decks) == Approx(0.75));
    CHECK(decks.advance(1.0));
    CHECK(decks.status().current == 0);
    CHECK(amountOf(decks) == Approx(0.0));
}

TEST_CASE("Decks: Sonderfälle") {
    Lab lab;
    const project::Project project = parse(lab, kProjectWithLooks);
    runtime::LiveDecks decks{lab.registry};
    REQUIRE(decks.start(project.looks[0], 0).ok());

    CHECK_FALSE(decks.go(1.0).ok());  // nichts vorgeladen
    REQUIRE(decks.arm(project.looks[1], 1).ok());
    CHECK_FALSE(decks.go(-1.0).ok());
    REQUIRE(decks.go(0.0).ok());  // Zeit 0: hartes Umschalten
    CHECK(decks.status().current == 1);
    CHECK_FALSE(decks.status().fading);
    CHECK(amountOf(decks) == Approx(1.0));

    // Neuer arm() mitten im Fade schließt den laufenden zuerst ab
    REQUIRE(decks.arm(project.looks[0], 0).ok());
    REQUIRE(decks.go(10.0).ok());
    CHECK(decks.advance(1.0));
    REQUIRE(decks.arm(project.looks[1], 1).ok());
    CHECK(decks.status().current == 0);
    CHECK(decks.status().armed == 1);
    CHECK(amountOf(decks) == Approx(0.0));

    // Dasselbe Look-Paar beliebig oft: die Zahl der Nodes wächst nicht
    for (int i = 0; i < 6; ++i) {
        REQUIRE(decks.arm(project.looks[i % 2], i % 2).ok());
        REQUIRE(decks.go(0.0).ok());
    }
    CHECK(decks.graph().nodes().size() <= 5);
    // Unbrauchbare Zeiten
    CHECK_FALSE(decks.advance(-1.0));
}
