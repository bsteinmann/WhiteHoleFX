// Tests der Cuelist in der Projektdatei (Issue #21): Lesen, Schreiben, Rundlauf, Fehlermeldungen.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using project::Cue;
using project::CueDmxProtocol;
using project::CueTrigger;

namespace {

struct Library {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    Library() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }
};

const std::string kHead = R"(whitehole-project 1
destination 0 127.0.0.1
fixture Wand
  matrix 4 2
  patch artnet destination 0 universe 0
end
node a wh.generator.plasma
end
output a.out

look Nebel fade 1
node n wh.generator.noise
end
output n.out
endlook

look "Bunte Welle" fade 3
node w wh.generator.plasma
end
output w.out
endlook
)";

}  // namespace

TEST_CASE("Cuelist: alle Auslöser werden gelesen", "[project][cue]") {
    Library lib;
    const auto parsed = project::parseProject(kHead + R"(
cue name "Intro" look Nebel trigger go fade 3 delay 0.5
cue look "Bunte Welle" trigger time 10 fade 2
cue look Nebel trigger key F5
cue look Nebel trigger dmx sacn 4 17 threshold 200 fade 0
cue look "Bunte Welle" trigger audio
cue fade 4
)",
                                              lib.registry);
    REQUIRE(parsed.ok());
    const auto& cues = parsed.value().cues;
    REQUIRE(cues.size() == 6);

    CHECK(cues[0].name == "Intro");
    CHECK(cues[0].look == "Nebel");
    CHECK(cues[0].trigger == CueTrigger::Go);
    CHECK(cues[0].fade == 3.0);
    CHECK(cues[0].delay == 0.5);
    CHECK(cues[1].trigger == CueTrigger::Time);
    CHECK(cues[1].time == 10.0);
    CHECK(cues[1].look == "Bunte Welle");
    CHECK(cues[2].trigger == CueTrigger::Key);
    CHECK(cues[2].key == "F5");
    CHECK(cues[3].trigger == CueTrigger::Dmx);
    CHECK(cues[3].dmxProtocol == CueDmxProtocol::Sacn);
    CHECK(cues[3].dmxUniverse == 4);
    CHECK(cues[3].dmxChannel == 17);
    CHECK(cues[3].dmxThreshold == 200);
    CHECK(cues[3].fade == 0.0);
    CHECK(cues[4].trigger == CueTrigger::Audio);
    CHECK(cues[5].look.empty());  // Schwarz
    CHECK(cues[5].fade == 4.0);
    CHECK(cues[5].trigger == CueTrigger::Go);  // Standard
}

TEST_CASE("Cuelist: schreiben und wieder lesen ergibt dieselbe Liste", "[project][cue]") {
    Library lib;
    const auto parsed = project::parseProject(kHead + R"(
cue name "Mit Leerzeichen" look "Bunte Welle" trigger key "Strg+1" delay 1.25 fade 0.1
cue look Nebel trigger dmx artnet 0 512 threshold 1
cue trigger time 3.5
cue trigger audio fade 0
cue name "Mit Notiz" note "Zeile eins\nZeile \"zwei\"" trigger go
)",
                                              lib.registry);
    REQUIRE(parsed.ok());
    const auto text = project::writeProject(parsed.value());
    REQUIRE(text.ok());
    const auto again = project::parseProject(text.value(), lib.registry);
    REQUIRE(again.ok());
    CHECK(again.value().cues == parsed.value().cues);
    CHECK(again.value().cues.size() == 5);
    CHECK(again.value().cues[4].note == "Zeile eins\nZeile \"zwei\"");
    // Ein Projekt ohne Cues schreibt keine Cue-Zeile
    const auto plain = project::parseProject(kHead, lib.registry);
    REQUIRE(plain.ok());
    CHECK(project::writeProject(plain.value()).value().find("cue") == std::string::npos);
}

TEST_CASE("Cuelist: Fehler nennen Zeile und Grund", "[project][cue]") {
    Library lib;
    const auto fails = [&](const std::string& cueLine) {
        const auto parsed = project::parseProject(kHead + cueLine + "\n", lib.registry);
        REQUIRE_FALSE(parsed.ok());
        return parsed.error().message;
    };
    CHECK(fails("cue look Gibtsnicht").find("Gibtsnicht") != std::string::npos);
    CHECK(fails("cue look Nebel trigger gleich").find("gleich") != std::string::npos);
    CHECK(fails("cue look Nebel fade -1").find("fade") != std::string::npos);
    CHECK(fails("cue look Nebel fade 99999").find("fade") != std::string::npos);
    CHECK(fails("cue look Nebel trigger dmx artnet 0 0").find("Kanal") != std::string::npos);
    CHECK(fails("cue look Nebel trigger dmx midi 0 1").find("midi") != std::string::npos);
    CHECK(fails("cue look Nebel trigger dmx artnet 0 600").find("Kanal") != std::string::npos);
    CHECK(fails("cue look Nebel trigger dmx artnet 0 1 threshold 0").find("Schwelle") != std::string::npos);
    CHECK(fails("cue look Nebel trigger key \"\"").find("Taste") != std::string::npos);
    CHECK(fails("cue look Nebel blubb 3").find("blubb") != std::string::npos);
    CHECK(fails("cue look Nebel trigger time").find("Wartezeit") != std::string::npos);
}

TEST_CASE("Cuelist: in einem look-Block ist keine cue-Zeile erlaubt", "[project][cue]") {
    Library lib;
    const auto parsed = project::parseProject(kHead + R"(
look X fade 1
node q wh.generator.plasma
end
output q.out
cue look X
endlook
)",
                                              lib.registry);
    CHECK_FALSE(parsed.ok());
}
