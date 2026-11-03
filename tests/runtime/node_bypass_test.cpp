// node_bypass_test.cpp: Überbrückung von Field-Nodes (graph, compiler, Projektdatei, Player mit Rückgängig).
//
// Ein überbrückter Node reicht seinen ersten Field-Eingang unverändert weiter; ohne Eingang liefert er
// Transparent. Er bleibt im Graphen, wird gespeichert und lässt sich zurücknehmen.
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "test_field_nodes.hpp"
#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
end
node spiegel wh.transform.mirror
end
node lfo wh.value.lfo
end
connect plasma.out -> spiegel.source
output spiegel.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    [[nodiscard]] NodeId id(const std::string& typeId) const {
        for (const auto& node : player.project()->graph.nodes()) {
            if (node.typeId == typeId) {
                return node.id;
            }
        }
        FAIL("Node nicht gefunden: " << typeId);
        return {};
    }
    [[nodiscard]] bool bypassed(const std::string& typeId) const {
        return player.project()->graph.findNode(id(typeId))->bypassed;
    }
};

}  // namespace

TEST_CASE("Compiler: ein überbrückter Node reicht seinen ersten Field-Eingang durch", "[bypass][compiler]") {
    const auto fixture = test::makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId source = graph.addNode("c.const", Vec2{}).value();
    const NodeId shift = graph.addNode("c.shift", Vec2{}).value();
    REQUIRE(graph.connect(graph::PortRef{source, "out"}, graph::PortRef{shift, "source"}).ok());
    REQUIRE(graph.setBypassed(shift, true).ok());

    const auto compiled = compiler::compileField(graph, fixture.library, graph::PortRef{shift, "out"});
    REQUIRE(compiled.ok());
    CHECK(contains(compiled.value().wgsl, "fn wh_f2(p: vec3<f32>) -> vec4<f32> {\n    return wh_f1(p);\n}"));
    CHECK_FALSE(contains(compiled.value().wgsl, "amount"));  // Parameter des Nodes entfallen

    REQUIRE(graph.setBypassed(shift, false).ok());
    const auto normal = compiler::compileField(graph, fixture.library, graph::PortRef{shift, "out"});
    REQUIRE(normal.ok());
    CHECK(contains(normal.value().wgsl, "amount"));
}

TEST_CASE("Compiler: überbrückt ohne Eingang oder Generator ergibt Transparent, Pflicht-Eingänge entfallen", "[bypass][compiler]") {
    const auto fixture = test::makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId generator = graph.addNode("c.const", Vec2{}).value();
    REQUIRE(graph.setBypassed(generator, true).ok());
    const auto flat = compiler::compileField(graph, fixture.library, graph::PortRef{generator, "out"});
    REQUIRE(flat.ok());
    CHECK(contains(flat.value().wgsl, "return vec4<f32>(0.0, 0.0, 0.0, 0.0);"));

    // c.shift hat einen Pflicht-Eingang ohne Kabel; überbrückt ist das kein Fehler.
    graph::Graph open{fixture.registry};
    const NodeId shift = open.addNode("c.shift", Vec2{}).value();
    CHECK_FALSE(compiler::compileField(open, fixture.library, graph::PortRef{shift, "out"}).ok());
    REQUIRE(open.setBypassed(shift, true).ok());
    const auto bypassed = compiler::compileField(open, fixture.library, graph::PortRef{shift, "out"});
    REQUIRE(bypassed.ok());
    CHECK(contains(bypassed.value().wgsl, "return vec4<f32>(0.0, 0.0, 0.0, 0.0);"));
}

TEST_CASE("Graph: nur Field-Nodes lassen sich überbrücken", "[bypass][graph]") {
    Rig rig;
    CHECK(rig.player.setNodeBypassed(rig.id("wh.value.lfo"), true).error().code == ErrorCode::Unsupported);
    CHECK(rig.player.setNodeBypassed(NodeId{9999}, true).error().code == ErrorCode::NotFound);
    CHECK_FALSE(rig.bypassed("wh.value.lfo"));
}

TEST_CASE("Player: Überbrücken ist rückgängig zu machen und ändert das Projekt", "[bypass][player]") {
    Rig rig;
    REQUIRE_FALSE(rig.player.modified());
    REQUIRE(rig.player.setNodeBypassed(rig.id("wh.transform.mirror"), true).ok());
    CHECK(rig.bypassed("wh.transform.mirror"));
    CHECK(rig.player.modified());
    REQUIRE(rig.player.canUndo());
    REQUIRE(rig.player.undo().ok());
    CHECK_FALSE(rig.bypassed("wh.transform.mirror"));
    // Derselbe Wert noch einmal ist kein Schritt.
    REQUIRE(rig.player.setNodeBypassed(rig.id("wh.transform.mirror"), false).ok());
}

TEST_CASE("Projektdatei: bypass wird geschrieben und wieder gelesen", "[bypass][project]") {
    Rig rig;
    REQUIRE(rig.player.setNodeBypassed(rig.id("wh.transform.mirror"), true).ok());
    const auto text = project::writeProject(*rig.player.project());
    REQUIRE(text.ok());
    CHECK(contains(text.value(), "\n  bypass\n"));

    Rig other;
    REQUIRE(other.player.loadText(text.value()).ok());
    CHECK(other.bypassed("wh.transform.mirror"));
    CHECK_FALSE(other.bypassed("wh.generator.plasma"));

    // Ein bypass-Zeile an einem CPU-Node ist ein Fehler mit Zeilenangabe.
    std::string broken = std::string{kProject};
    const std::string marker = "node lfo wh.value.lfo\n";
    broken.insert(broken.find(marker) + marker.size(), "  bypass\n");
    CHECK_FALSE(other.player.loadText(broken).ok());
}

TEST_CASE("Player: mehrere Nodes überbrücken ist ein Schritt, alles oder nichts", "[bypass][player]") {
    Rig rig;
    const std::vector<NodeId> both{rig.id("wh.generator.plasma"), rig.id("wh.transform.mirror")};
    REQUIRE(rig.player.setNodesBypassed(both, true).ok());
    CHECK(rig.bypassed("wh.generator.plasma"));
    CHECK(rig.bypassed("wh.transform.mirror"));
    REQUIRE(rig.player.undo().ok());  // ein einziger Schritt nimmt beide zurück
    CHECK_FALSE(rig.bypassed("wh.generator.plasma"));
    CHECK_FALSE(rig.bypassed("wh.transform.mirror"));

    // Ein CPU-Node in der Liste lässt die ganze Änderung scheitern.
    const std::vector<NodeId> mixed{rig.id("wh.transform.mirror"), rig.id("wh.value.lfo")};
    CHECK_FALSE(rig.player.setNodesBypassed(mixed, true).ok());
    CHECK_FALSE(rig.bypassed("wh.transform.mirror"));
}
