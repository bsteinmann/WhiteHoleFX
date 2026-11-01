// Tests der Projektdatei: Lesen, Schreiben, Fehlermeldungen.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using project::parseProject;
using project::writeProject;

namespace {

struct Library {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    Library() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }
};

const char* const kSample = R"(whitehole-project 1
# Beispiel: Wand aus zwei Panels, Strip an der Decke, Würfel

settings fps 30 normalize keep-aspect sacn-priority 120 source-name "Meine Wand"

destination 0 192.168.1.50
destination 1 multicast
destination 2 10.0.0.7 6455

fixture "Panel links" at 0 0 0
  matrix 16 8 pitch 1 1 start bottom-right first columns serpentine
  patch artnet destination 0 universe 3 channel 1 order GRB gamma 2.5 brightness 0.5
end

fixture Decke at 0 12 0
  strip 60 from 0 0 0 to 32 0 0
  patch sacn destination 1 universe 7 channel 1
end

fixture Wuerfel at 40 0 0
  grid 4 4 4 pitch 2 2 2 order ZYX serpentine flipx flipz
  patch ddp destination 2 offset 480 order BGR
end

fixture Punkte
  point 0 0 0
  point 1.5 2 0.25
end

node a wh.generator.gradient
  set angle 30
  set colorA 1 0 0 1
end
node b wh.transform.mirror
  set mirrorY true
end
connect a.out -> b.source
output b.out
)";

std::string ErrorOf(const std::string& text, const Library& lib) {
    const auto result = parseProject(text, lib.registry);
    REQUIRE_FALSE(result.ok());
    return result.error().message;
}

std::string withHeader(const std::string& body) { return "whitehole-project 1\n" + body; }

}  // namespace

TEST_CASE("Projekt: das Beispiel wird vollständig gelesen", "[project]") {
    const Library lib;
    const auto result = parseProject(kSample, lib.registry);
    REQUIRE(result.ok());
    const project::Project& p = result.value();

    CHECK(p.framesPerSecond == 30.0);
    CHECK(p.normalize == mapping::NormalizeMode::KeepAspect);
    CHECK(p.sacnPriority == 120);
    CHECK(p.sourceName == "Meine Wand");

    REQUIRE(p.destinations.size() == 3);
    CHECK(p.destinations[0].address == "192.168.1.50");
    CHECK(p.destinations[0].port == 0);
    CHECK(p.destinations[1].address.empty());
    CHECK(p.destinations[2].port == 6455);

    REQUIRE(p.layout.fixtures().size() == 4);
    const auto& panel = p.layout.fixtures()[0];
    CHECK(panel.name == "Panel links");
    const auto* matrix = std::get_if<mapping::MatrixShape>(&panel.shape);
    REQUIRE(matrix != nullptr);
    CHECK(matrix->columns == 16);
    CHECK(matrix->rows == 8);
    CHECK(matrix->wiring.start == mapping::StartCorner::BottomRight);
    CHECK(matrix->wiring.first == mapping::FirstAxis::Columns);
    CHECK(matrix->wiring.serpentine);
    CHECK(p.layout.fixtures()[1].origin == Vec3{0.0F, 12.0F, 0.0F});
    const auto* cube = std::get_if<mapping::GridShape>(&p.layout.fixtures()[2].shape);
    REQUIRE(cube != nullptr);
    CHECK(cube->wiring.flipX);
    CHECK_FALSE(cube->wiring.flipY);
    CHECK(cube->wiring.flipZ);
    const auto* points = std::get_if<mapping::PointsShape>(&p.layout.fixtures()[3].shape);
    REQUIRE(points != nullptr);
    CHECK(points->points.size() == 2);
    CHECK(points->points[1] == Vec3{1.5F, 2.0F, 0.25F});

    REQUIRE(p.patches.size() == 3);
    CHECK(p.patches[0].protocol == output::Protocol::ArtNet);
    CHECK(p.patches[0].universe == 3);
    CHECK(p.patches[0].color.order == output::ColorOrder::GRB);
    CHECK(p.patches[0].color.gamma == 2.5F);
    CHECK(p.patches[0].color.brightness == 0.5F);
    CHECK(p.patches[1].protocol == output::Protocol::Sacn);
    CHECK(p.patches[2].protocol == output::Protocol::Ddp);
    CHECK(p.patches[2].ddpOffset == 480);
    CHECK(p.patches[2].fixture == p.layout.fixtures()[2].id);

    REQUIRE(p.graph.nodes().size() == 2);
    REQUIRE(p.graph.connections().size() == 1);
    const graph::NodeInstance& gradient = p.graph.nodes()[0];
    CHECK(std::get<double>(gradient.parameters.at("angle")) == 30.0);
    CHECK(std::get<ColorRgba>(gradient.parameters.at("colorA")) == ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    CHECK(std::get<bool>(p.graph.nodes()[1].parameters.at("mirrorY")));
    REQUIRE(p.target.has_value());
    CHECK(p.target->node == p.graph.nodes()[1].id);
    CHECK(p.target->portKey == "out");

    // Alles, was das Layout hergibt, ist auch tatsächlich bakebar und patchbar.
    const auto map = p.layout.bake(p.normalize);
    REQUIRE(map.ok());
    CHECK(map.value().world.size() == 16U * 8U + 60U + 64U + 2U);
}

TEST_CASE("Projekt: Schreiben und wieder Lesen ergibt denselben Text", "[project]") {
    const Library lib;
    const auto first = parseProject(kSample, lib.registry);
    REQUIRE(first.ok());
    const auto text = writeProject(first.value());
    REQUIRE(text.ok());
    const auto second = parseProject(text.value(), lib.registry);
    REQUIRE(second.ok());
    const auto again = writeProject(second.value());
    REQUIRE(again.ok());
    CHECK(text.value() == again.value());

    // Auch die Werte stimmen, nicht nur der Text.
    CHECK(second.value().patches.size() == first.value().patches.size());
    CHECK(second.value().layout.fixtures().size() == first.value().layout.fixtures().size());
    CHECK(second.value().sourceName == "Meine Wand");
    CHECK(second.value().framesPerSecond == 30.0);
}

TEST_CASE("Projekt: ein leeres Projekt ist gültig", "[project]") {
    const Library lib;
    const auto result = parseProject("whitehole-project 1\n", lib.registry);
    REQUIRE(result.ok());
    CHECK(result.value().layout.fixtures().empty());
    CHECK_FALSE(result.value().target.has_value());
    CHECK(result.value().framesPerSecond == 40.0);
}

TEST_CASE("Projekt: Kommentare, Leerzeilen und Windows-Zeilenenden", "[project]") {
    const Library lib;
    const std::string text =
        "# vorab\r\n\r\nwhitehole-project 1   # Kopf\r\nfixture A # Name\r\n  strip 3 from 0 0 0 to 3 0 0\r\nend\r\n";
    const auto result = parseProject(text, lib.registry);
    REQUIRE(result.ok());
    CHECK(result.value().layout.fixtures().size() == 1);
}

TEST_CASE("Projekt: Namen mit Leerzeichen, Anführungszeichen und Umlauten bleiben erhalten", "[project]") {
    const Library lib;
    project::Project p{lib.registry};
    p.sourceName = "Wand \"Süd\" # 2";
    REQUIRE(p.layout.add("Panel \"A\"\\1 ö", Vec3{}, mapping::StripShape{4, {}, {4.0F, 0.0F, 0.0F}}).ok());
    const auto text = writeProject(p);
    REQUIRE(text.ok());
    const auto back = parseProject(text.value(), lib.registry);
    REQUIRE(back.ok());
    CHECK(back.value().sourceName == p.sourceName);
    CHECK(back.value().layout.fixtures()[0].name == "Panel \"A\"\\1 ö");
}

TEST_CASE("Projekt: Fließkommazahlen überstehen die Rundreise bitgenau", "[project]") {
    const Library lib;
    project::Project p{lib.registry};
    p.framesPerSecond = 29.97;
    REQUIRE(p.layout.add("A", Vec3{0.1F, 1.0F / 3.0F, 1e-7F}, mapping::StripShape{2, {}, {0.3F, 0.0F, 0.0F}}).ok());
    const auto back = parseProject(writeProject(p).value(), lib.registry);
    REQUIRE(back.ok());
    CHECK(back.value().framesPerSecond == 29.97);
    CHECK(back.value().layout.fixtures()[0].origin == Vec3{0.1F, 1.0F / 3.0F, 1e-7F});
}

TEST_CASE("Projekt: Fehlermeldungen nennen die Zeile und den Grund", "[project]") {
    const Library lib;
    struct Case {
        const char* text;
        const char* expected;
    };
    const Case cases[] = {
        {"", "leer"},
        {"fixture A\nend\n", "Zeile 1: Die Datei muss mit"},
        {"whitehole-project 2\n", "Zeile 1: Formatversion 2"},
        {"whitehole-project 1\nblubb\n", "Zeile 2: Unbekannte Anweisung \"blubb\""},
        {"whitehole-project 1\nsettings fps 0\n", "Zeile 2: fps muss"},
        {"whitehole-project 1\nsettings fps abc\n", "keine endliche Zahl"},
        {"whitehole-project 1\nsettings fps nan\n", "keine endliche Zahl"},
        {"whitehole-project 1\nsettings normalize rund\n", "erlaubt: stretch, keep-aspect"},
        {"whitehole-project 1\nsettings sacn-priority 201\n", "von 0 bis 200"},
        {"whitehole-project 1\ndestination 1 10.0.0.1\n", "erwartet 0"},
        {"whitehole-project 1\ndestination 0 10.0.0\n", "keine IPv4-Adresse"},
        {"whitehole-project 1\ndestination 0 10.0.0.1 70000\n", "Port"},
        {"whitehole-project 1\nfixture \"A\n", "Zeile 2: Anführungszeichen nicht geschlossen"},
        {"whitehole-project 1\nfixture A\n  matrix 4 4\n", "Zeile 2: Block ist nicht mit \"end\" beendet"},
        {"whitehole-project 1\nfixture A\nend\n", "Es fehlt die Form"},
        {"whitehole-project 1\nfixture A\n  matrix 4 4\n  strip 2 from 0 0 0 to 1 0 0\nend\n", "schon eine Form"},
        {"whitehole-project 1\nfixture A\n  point 0 0 0\n  matrix 2 2\nend\n", "schon eine Form"},
        {"whitehole-project 1\nfixture A\n  matrix 0 4\nend\n", "Zeile 2 (fixture \"A\")"},
        {"whitehole-project 1\nfixture A\n  matrix 4 4 pitch 1\nend\n", "Zeile 3: Abstand fehlt"},
        {"whitehole-project 1\nfixture A\n  matrix 4 4 start mitte\nend\n", "erlaubt: top-left"},
        {"whitehole-project 1\nfixture A\n  matrix 4 4 farbe\nend\n", "Unbekannte Option \"farbe\" bei matrix"},
        {"whitehole-project 1\nfixture A\n  rund 4\nend\n", "Zeile 3: Unbekannte Anweisung \"rund\""},
        {"whitehole-project 1\nfixture A\n  strip 2 from 0 0 0 bis 1 0 0\nend\n", "Erwartet \"to\""},
        {"whitehole-project 1\nfixture A\n  matrix 2 2\n  patch artnet universe 1\nend\n", "braucht \"destination"},
        {"whitehole-project 1\nfixture A\n  matrix 2 2\n  patch lora destination 0\nend\n", "erlaubt: artnet, sacn, ddp"},
        {"whitehole-project 1\nfixture A\n  matrix 2 2\nend\nfixture A\n  matrix 2 2\nend\n", "Name ist schon vergeben"},
        {"whitehole-project 1\nnode a wh.gibt.es.nicht\n", "Zeile 2"},
        {"whitehole-project 1\nnode a.b wh.generator.gradient\n", "keinen Punkt"},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\nnode a wh.generator.gradient\n", "schon vergeben"},
        {"whitehole-project 1\nnode a wh.generator.gradient\n  set tempo 3\nend\n", "keinen Parameter \"tempo\""},
        {"whitehole-project 1\nnode a wh.generator.gradient\n  set angle zwei\nend\n", "keine endliche Zahl"},
        {"whitehole-project 1\nnode a wh.generator.gradient\n  set colorA 1 0 0\nend\n", "Zeile 3: colorA fehlt"},
        {"whitehole-project 1\nnode a wh.transform.mirror\n  set mirrorX vielleicht\nend\n", "true oder false"},
        {"whitehole-project 1\nnode a wh.generator.gradient\n  tu 1\nend\n", "im node-Block"},
        {"whitehole-project 1\nnode a wh.generator.gradient\n  at 5\nend\n", "Zeile 3: Position fehlt"},
        {"whitehole-project 1\nnode a wh.generator.gradient\n  at 1 2 3\nend\n", "Zeile 3"},
        {"whitehole-project 1\nconnect a.out -> b.source\n", "Unbekannter Node \"a\""},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\nconnect a.out b.source\n", "Erwartet"},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\nconnect a.out ->\n", "Eingang fehlt"},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\nconnect aout -> a.source\n", "erwartet <node>.<port>"},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\nnode b wh.transform.mirror\nend\n"
         "connect a.out -> b.source\nconnect a.out -> b.source\n", "Zeile 7"},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\noutput a.source\n", "kein Ausgang"},
        {"whitehole-project 1\nnode a wh.generator.gradient\nend\noutput a.out\noutput a.out\n", "schon gesetzt"},
    };
    for (const Case& c : cases) {
        INFO("Eingabe:\n" << c.text);
        CHECK(ErrorOf(c.text, lib).find(c.expected) != std::string::npos);
    }
}

TEST_CASE("Projekt: Zyklen im Graphen werden beim Lesen gemeldet", "[project]") {
    const Library lib;
    const std::string text = withHeader(
        "node a wh.transform.mirror\nend\nnode b wh.transform.mirror\nend\n"
        "connect a.out -> b.source\nconnect b.out -> a.source\n");
    const auto result = parseProject(text, lib.registry);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::CycleDetected);
    CHECK(result.error().message.find("Zeile 7") != std::string::npos);
}

TEST_CASE("Projekt: Fehlercodes unterscheiden Syntax, Unbekanntes und Version", "[project]") {
    const Library lib;
    CHECK(parseProject("whitehole-project 9\n", lib.registry).error().code == ErrorCode::Unsupported);
    CHECK(parseProject(withHeader("node a wh.nix\n"), lib.registry).error().code == ErrorCode::NotFound);
    CHECK(parseProject(withHeader("blubb\n"), lib.registry).error().code == ErrorCode::InvalidArgument);
    CHECK(parseProject(withHeader("fixture A\n matrix 2 2\nend\nfixture A\n matrix 2 2\nend\n"), lib.registry)
              .error()
              .code == ErrorCode::AlreadyExists);
}

TEST_CASE("Projekt: ein Patch auf ein fremdes Fixture lässt sich nicht schreiben", "[project]") {
    const Library lib;
    project::Project p{lib.registry};
    output::FixturePatch patch;
    patch.fixture = mapping::FixtureId{5};
    p.patches.push_back(patch);
    CHECK_FALSE(writeProject(p).ok());
}

TEST_CASE("Projekt: readTextFile meldet fehlende Dateien", "[project]") {
    const auto result = project::readTextFile("/nicht/vorhanden.whfx");
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::IoError);
}

TEST_CASE("Projekt: writeTextFile schreibt und ersetzt", "[project]") {
    const std::string path = (std::filesystem::temp_directory_path() / "whfx_write_text_test.txt").string();
    REQUIRE(project::writeTextFile(path, "eins zwei drei\n").ok());
    REQUIRE(project::writeTextFile(path, "neu").ok());
    const auto back = project::readTextFile(path);
    REQUIRE(back.ok());
    CHECK(back.value() == "neu");
    std::filesystem::remove(path);
    const auto bad = project::writeTextFile("/nicht/vorhanden/x.txt", "x");
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::IoError);
}

TEST_CASE("Projekt: Node-Positionen (at) werden gelesen und geschrieben", "[project]") {
    const Library lib;
    const auto parsed = parseProject("whitehole-project 1\nnode a wh.generator.gradient\n  at -12.5 40\nend\n", lib.registry);
    REQUIRE(parsed.ok());
    CHECK(parsed.value().graph.nodes().at(0).position == Vec2{-12.5F, 40.0F});
    const auto text = writeProject(parsed.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("  at -12.5 40\n") != std::string::npos);
    // Dateien ohne at (Format wie bisher) bleiben lesbar
    const auto old = parseProject("whitehole-project 1\nnode a wh.generator.gradient\nend\n", lib.registry);
    REQUIRE(old.ok());
    CHECK(old.value().graph.nodes().at(0).position == Vec2{});
}
