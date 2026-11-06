// Tests der Ziele von Output-Nodes im Player: Rechteck folgt dem Layout, Undo, Umbenennen, Fehler.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
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
