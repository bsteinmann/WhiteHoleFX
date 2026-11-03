// node_docking_player_test.cpp: Player::addNodeDocked legt einen Node an und verdrahtet ihn in einem Rückgängig-Schritt.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/graph/node_docking.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

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
    [[nodiscard]] std::size_t nodeCount() const { return player.project()->graph.nodes().size(); }
    [[nodiscard]] std::size_t wireCount() const { return player.project()->graph.connections().size(); }
};

}  // namespace

TEST_CASE("addNodeDocked fügt einen Node in ein Kabel ein, Rückgängig nimmt alles zurück") {
    Rig rig;
    const graph::Graph& g = rig.player.project()->graph;
    graph::DockTarget target;
    target.kind = graph::DockTarget::Kind::Wire;
    target.wire = g.connections().front().id;
    const graph::NodeDescriptor* colormap = g.registry().find("wh.modifier.colormap");
    REQUIRE(colormap != nullptr);
    const auto plan = graph::planDocking(g, *colormap, target);
    REQUIRE(plan.has_value());

    const auto id = rig.player.addNodeDocked("wh.modifier.colormap", Vec2{10.0F, 20.0F}, *plan);
    REQUIRE(id.ok());
    CHECK(rig.nodeCount() == 3);
    CHECK(rig.wireCount() == 2);  // plasma -> colormap -> spiegel
    const graph::NodeInstance* node = rig.player.project()->graph.findNode(id.value());
    REQUIRE(node != nullptr);
    CHECK(node->position.x == 10.0F);

    REQUIRE(rig.player.undo().ok());  // ein Schritt
    CHECK(rig.nodeCount() == 2);
    CHECK(rig.wireCount() == 1);
    REQUIRE(rig.player.redo().ok());
    CHECK(rig.nodeCount() == 3);
    CHECK(rig.wireCount() == 2);
}

TEST_CASE("addNodeDocked: ein ungültiger Plan ändert nichts") {
    Rig rig;
    graph::DockPlan plan;
    plan.links.push_back({graph::PortRef{NodeId{}, "gibt_es_nicht"}, graph::PortRef{rig.id("wh.transform.mirror"), "source"}});
    const auto id = rig.player.addNodeDocked("wh.generator.plasma", Vec2{}, plan);
    CHECK_FALSE(id.ok());
    CHECK(rig.nodeCount() == 2);
    CHECK(rig.wireCount() == 1);
    CHECK_FALSE(rig.player.addNodeDocked("does.not.exist", Vec2{}, graph::DockPlan{}).ok());
    CHECK_FALSE(rig.player.addNodeDocked("wh.output.canvas", Vec2{}, graph::DockPlan{}).ok());
    CHECK(rig.nodeCount() == 2);
}
