// Tests der Ziele von Output-Nodes im Player: Rechteck folgt dem Layout, Undo, Umbenennen, Fehler.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/graph/output_merge.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
};

}  // namespace

TEST_CASE("Ein Output-Ziel setzt das Rechteck und folgt dem Layout", "[runtime][target]") {
    Rig rig;
    REQUIRE(rig.player.loadText(R"(whitehole-project 1
destination 0 10.0.0.5
fixture "Matrix 1" at 0 0 0
  matrix 4 4 pitch 1 1
  patch artnet destination 0 universe 1
end
fixture "Matrix 2" at 10 0 0
  matrix 4 4 pitch 1 1
  patch artnet destination 0 universe 2
end
node g wh.generator.gradient
end
node a wh.output.canvas
end
connect g.out -> a.source
output a.out
)").ok());
    runtime::Player& player = rig.player;
    const NodeId node = player.project()->graph.nodes()[1].id;
    REQUIRE(player.setOutputTargets(node, {"fixture:Matrix 2"}).ok());
    const auto param = [&](const char* key) {
        return static_cast<float>(std::get<double>(player.project()->graph.findNode(node)->parameters.at(key)));
    };
    CHECK(param("left") > 0.5F);

    // Umbenennen: das Ziel zieht mit.
    mapping::FixtureId second;
    for (const mapping::Fixture& f : player.project()->layout.fixtures()) {
        if (f.name == "Matrix 2") {
            second = f.id;
        }
    }
    REQUIRE(player.renameFixture(second, "Rechts").ok());
    CHECK(player.project()->graph.findNode(node)->targets == std::vector<std::string>{"fixture:Rechts"});

    // Verschieben: das Rechteck wird neu berechnet (Matrix 2 rückt links neben Matrix 1 → ganz links).
    REQUIRE(player.moveFixture(second, Vec3{-10.0F, 0.0F, 0.0F}).ok());
    CHECK(param("left") < 0.1F);

    // Undo nimmt das Verschieben samt Rechteck zurück.
    REQUIRE(player.undo().ok());
    CHECK(param("left") > 0.5F);

    CHECK_FALSE(player.setOutputTargets(NodeId{9999}, {"fixture:Rechts"}).ok());
    CHECK_FALSE(player.setGroups({project::FixtureGroup{"A", {}}}).ok());
    CHECK_FALSE(player.setRegions({project::StageRegion{"R", 0.5F, 0.0F, 0.9F, 1.0F}}).ok());
}

TEST_CASE("Zwei Output-Nodes sind gleichzeitig aktiv", "[runtime][target]") {
    Rig rig;
    REQUIRE(rig.player.loadText(R"(whitehole-project 1
destination 0 10.0.0.5
fixture "Links" at 0 0 0
  matrix 4 4 pitch 1 1
  patch artnet destination 0 universe 1
end
fixture "Rechts" at 10 0 0
  matrix 4 4 pitch 1 1
  patch artnet destination 0 universe 2
end
node g1 wh.generator.gradient
end
node o1 wh.output.canvas
end
connect g1.out -> o1.source
output o1.out
)").ok());
    runtime::Player& player = rig.player;
    const NodeId first = player.project()->graph.nodes()[1].id;
    const Result<NodeId> second = player.addNode("wh.output.canvas", Vec2{0.0F, 100.0F});
    REQUIRE(second.ok());
    // Der Ausgang liegt beim neuen Output; es entstehen keine Kabel zwischen den Outputs (der Adder ist unsichtbar).
    CHECK(player.project()->target->node == second.value());
    CHECK(player.project()->graph.findInputConnection(second.value(), "behind") == nullptr);
    CHECK(player.project()->graph.findInputConnection(second.value(), "source") == nullptr);
    CHECK(player.project()->graph.findInputConnection(first, "behind") == nullptr);

    // Den Ausgang auf den ersten Output zu setzen, ändert nichts an den Kabeln.
    REQUIRE(player.setTarget(graph::PortRef{first, "out"}).ok());
    CHECK(player.project()->graph.connections().size() == 1);
}

TEST_CASE("Der unsichtbare Adder fasst parallele Outputs zusammen und übersteht fehlende Quellen", "[runtime][target]") {
    Rig rig;
    REQUIRE(rig.player.loadText(R"(whitehole-project 1
destination 0 10.0.0.5
fixture "Links" at 0 0 0
  matrix 4 4 pitch 1 1
  patch artnet destination 0 universe 1
end
node g1 wh.generator.gradient
end
node g2 wh.generator.gradient
end
node o1 wh.output.canvas
end
node o2 wh.output.canvas
end
connect g1.out -> o1.source
connect g2.out -> o2.source
output o2.out
)").ok());
    const graph::Graph& g = rig.player.project()->graph;
    const std::size_t nodesBefore = g.nodes().size();
    const auto merged = graph::mergeOutputs(g, *rig.player.project()->target);
    REQUIRE(merged.has_value());
    CHECK(merged->graph.nodes().size() == nodesBefore + 1);  // ein Add-Node, nur auf der Kopie
    CHECK(g.nodes().size() == nodesBefore);
    CHECK(merged->graph.findNode(merged->target.node)->typeId == "wh.blend.add");

    // Fällt die Quelle von o1 weg, liefert o2 allein weiter (kein Add nötig, Ausgang bleibt o2).
    const graph::Connection* source = g.findInputConnection(g.nodes()[2].id, "source");
    REQUIRE(source != nullptr);
    REQUIRE(rig.player.disconnect(source->id).ok());
    CHECK_FALSE(graph::mergeOutputs(rig.player.project()->graph, *rig.player.project()->target).has_value());

    // Ist der Ausgang des Projekts ein leerer Output, springt der Adder auf den Output mit Effekt.
    const auto fallback = graph::mergeOutputs(rig.player.project()->graph,
                                              graph::PortRef{rig.player.project()->graph.nodes()[2].id, "out"});
    REQUIRE(fallback.has_value());
    CHECK(fallback->target.node == rig.player.project()->graph.nodes()[3].id);
}
