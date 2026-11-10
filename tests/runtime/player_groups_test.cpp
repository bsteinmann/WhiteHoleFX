// Tests der Gruppen-Operationen im Player: Anlegen, Ändern, Auflösen, Zuklappen und Verschieben, jeweils mit Rückgängig.
#include <catch2/catch_test_macros.hpp>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
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

    [[nodiscard]] graph::NodeGroup group(const std::string& name) const {
        graph::NodeGroup g;
        g.name = name;
        for (const auto& node : player.project()->graph.nodes()) {
            g.members.push_back(node.id);
        }
        return g;
    }
    [[nodiscard]] const graph::Graph& graph() const { return player.project()->graph; }
};

}  // namespace

TEST_CASE("Player: Gruppe anlegen und mit Rückgängig entfernen", "[player][groups]") {
    Rig rig;
    REQUIRE(rig.player.addNodeGroup(rig.group("Plasma")).ok());
    CHECK(rig.graph().groups().size() == 1);
    CHECK(rig.player.modified());
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.graph().groups().empty());
    REQUIRE(rig.player.redo().ok());
    CHECK(rig.graph().findGroup("Plasma") != nullptr);
}

TEST_CASE("Player: doppelter Name und leere Gruppe scheitern ohne Änderung", "[player][groups]") {
    Rig rig;
    REQUIRE(rig.player.addNodeGroup(rig.group("A")).ok());
    CHECK_FALSE(rig.player.addNodeGroup(rig.group("A")).ok());
    graph::NodeGroup empty;
    empty.name = "Leer";
    CHECK_FALSE(rig.player.addNodeGroup(empty).ok());
    CHECK(rig.graph().groups().size() == 1);
}

TEST_CASE("Player: Gruppe ändern, zuklappen, auflösen", "[player][groups]") {
    Rig rig;
    REQUIRE(rig.player.addNodeGroup(rig.group("A")).ok());
    graph::NodeGroup changed = *rig.graph().findGroup("A");
    changed.name = "B";
    REQUIRE(rig.player.replaceNodeGroup("A", changed).ok());
    CHECK(rig.graph().findGroup("A") == nullptr);
    REQUIRE(rig.graph().findGroup("B") != nullptr);

    REQUIRE(rig.player.setNodeGroupCollapsed("B", false).ok());
    CHECK_FALSE(rig.graph().findGroup("B")->collapsed);

    REQUIRE(rig.player.removeNodeGroup("B").ok());
    CHECK(rig.graph().groups().empty());
    CHECK(rig.graph().nodes().size() == 2);  // die Nodes bleiben
    CHECK_FALSE(rig.player.removeNodeGroup("B").ok());
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.graph().findGroup("B") != nullptr);
}

TEST_CASE("Player: Zuklappen merkt die Ecke, Aufklappen holt die Nodes zur Kachel", "[player][groups]") {
    Rig rig;
    const NodeId first = rig.graph().nodes().front().id;
    const NodeId second = rig.graph().nodes().back().id;
    REQUIRE(rig.player.moveNode(first, Vec2{100.0F, 50.0F}).ok());
    REQUIRE(rig.player.moveNode(second, Vec2{300.0F, 80.0F}).ok());
    REQUIRE(rig.player.addNodeGroup(rig.group("A")).ok());

    REQUIRE(rig.player.setNodeGroupCollapsed("A", true).ok());
    CHECK(rig.graph().findGroup("A")->position.x == 100.0F);
    CHECK(rig.graph().findGroup("A")->position.y == 50.0F);

    REQUIRE(rig.player.moveNodeGroup("A", Vec2{500.0F, 500.0F}).ok());
    REQUIRE(rig.player.setNodeGroupCollapsed("A", false).ok());
    CHECK(rig.graph().findNode(first)->position.x == 500.0F);
    CHECK(rig.graph().findNode(first)->position.y == 500.0F);
    CHECK(rig.graph().findNode(second)->position.x == 700.0F);  // Abstand bleibt
    CHECK(rig.graph().findNode(second)->position.y == 530.0F);

    REQUIRE(rig.player.undo().ok());  // ein Schritt macht das Aufklappen samt Verschieben rückgängig
    CHECK(rig.graph().findGroup("A")->collapsed);
    CHECK(rig.graph().findNode(first)->position.x == 100.0F);
}

TEST_CASE("Player: Ziehen der Kachel ist ein Schritt Rückgängig", "[player][groups]") {
    Rig rig;
    REQUIRE(rig.player.addNodeGroup(rig.group("A")).ok());
    for (float x = 1.0F; x <= 5.0F; x += 1.0F) {
        REQUIRE(rig.player.moveNodeGroup("A", Vec2{x * 10.0F, 0.0F}).ok());
    }
    CHECK(rig.graph().findGroup("A")->position.x == 50.0F);
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.graph().findGroup("A")->position.x == 0.0F);
    CHECK(rig.graph().findGroup("A") != nullptr);
    CHECK_FALSE(rig.player.moveNodeGroup("fehlt", Vec2{}).ok());
}
