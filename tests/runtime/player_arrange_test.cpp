// Tests: mehrere Fixtures bewegen, ausrichten, verteilen und der Effektbereich im Player.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;
using runtime::Player;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture A at 0 0 0
  matrix 4 2
  patch artnet destination 0 universe 0
end
fixture B at 10 3 0
  matrix 2 2
  patch artnet destination 0 universe 1
end
fixture C at 5 -4 0
  matrix 2 1
  patch artnet destination 0 universe 2
end
node g wh.generator.gradient
end
output g.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    [[nodiscard]] mapping::FixtureId id(std::size_t index) const {
        return player.project()->layout.fixtures().at(index).id;
    }
    [[nodiscard]] std::vector<mapping::FixtureId> all() const { return {id(0), id(1), id(2)}; }
    [[nodiscard]] Vec3 origin(std::size_t index) const { return player.project()->layout.fixtures().at(index).origin; }
};

}  // namespace

TEST_CASE("Anordnen: mehrere Fixtures verschieben ist ein Undo-Schritt", "[player][arrange]") {
    Rig rig;
    REQUIRE(rig.player.moveFixtures({rig.id(0), rig.id(1)}, Vec3{1.0F, 2.0F, 0.0F}).ok());
    CHECK(rig.origin(0) == Vec3{1.0F, 2.0F, 0.0F});
    CHECK(rig.origin(1) == Vec3{11.0F, 5.0F, 0.0F});
    CHECK(rig.origin(2) == Vec3{5.0F, -4.0F, 0.0F});
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.origin(0) == Vec3{});
    CHECK(rig.origin(1) == Vec3{10.0F, 3.0F, 0.0F});
}

TEST_CASE("Anordnen: Fehler lassen alles unverändert", "[player][arrange]") {
    Rig rig;
    CHECK(rig.player.moveFixtures({mapping::FixtureId{99}}, Vec3{1.0F, 0.0F, 0.0F}).error().code == ErrorCode::NotFound);
    CHECK_FALSE(rig.player.moveFixtures({rig.id(0)}, Vec3{std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F}).ok());
    CHECK_FALSE(rig.player.alignFixtures({}, Player::Align::Left).ok());
    CHECK_FALSE(rig.player.distributeFixtures({rig.id(0), rig.id(1)}, Player::Axis::X).ok());
    CHECK_FALSE(rig.player.canUndo());
    CHECK(rig.origin(0) == Vec3{});
}

TEST_CASE("Anordnen: links, rechts, oben, unten und Mitte", "[player][arrange]") {
    Rig rig;  // Boxen: A x0..4 y0..2, B x10..12 y3..5, C x5..7 y-4..-3
    REQUIRE(rig.player.alignFixtures(rig.all(), Player::Align::Left).ok());
    CHECK(rig.origin(0).x == Approx(0.0F));
    CHECK(rig.origin(1).x == Approx(0.0F));
    CHECK(rig.origin(2).x == Approx(0.0F));
    CHECK(rig.origin(1).y == Approx(3.0F));  // die andere Achse bleibt
    REQUIRE(rig.player.undo().ok());

    REQUIRE(rig.player.alignFixtures(rig.all(), Player::Align::Right).ok());
    CHECK(rig.origin(0).x == Approx(8.0F));  // rechte Kante 12
    CHECK(rig.origin(1).x == Approx(10.0F));
    CHECK(rig.origin(2).x == Approx(10.0F));
    REQUIRE(rig.player.undo().ok());

    REQUIRE(rig.player.alignFixtures(rig.all(), Player::Align::Top).ok());
    CHECK(rig.origin(0).y == Approx(3.0F));  // obere Kante 5
    CHECK(rig.origin(1).y == Approx(3.0F));
    CHECK(rig.origin(2).y == Approx(4.0F));  // Höhe 1: Ursprung 4 -> Kante 5
    REQUIRE(rig.player.undo().ok());

    REQUIRE(rig.player.alignFixtures(rig.all(), Player::Align::Bottom).ok());
    CHECK(rig.origin(0).y == Approx(-4.0F));
    CHECK(rig.origin(1).y == Approx(-4.0F));
    CHECK(rig.origin(2).y == Approx(-4.0F));
    REQUIRE(rig.player.undo().ok());

    REQUIRE(rig.player.alignFixtures(rig.all(), Player::Align::HCenter).ok());  // Mitte x = 6
    CHECK(rig.origin(0).x == Approx(4.0F));
    CHECK(rig.origin(1).x == Approx(5.0F));
    CHECK(rig.origin(2).x == Approx(5.0F));
    REQUIRE(rig.player.undo().ok());

    REQUIRE(rig.player.alignFixtures(rig.all(), Player::Align::VCenter).ok());  // Mitte y = 0.5
    CHECK(rig.origin(0).y == Approx(-0.5F));
    CHECK(rig.origin(1).y == Approx(-0.5F));
    CHECK(rig.origin(2).y == Approx(0.0F));
}

TEST_CASE("Anordnen: gleichmäßig verteilen", "[player][arrange]") {
    Rig rig;  // x-Boxen: A 0..4, C 5..7, B 10..12 -> Lücken 1 und 3, Summe der Breiten 8
    REQUIRE(rig.player.distributeFixtures(rig.all(), Player::Axis::X).ok());
    // Spanne 12, Breiten 4+2+2 = 8, Lücke (12-8)/2 = 2: A 0..4, C 6..8, B 10..12
    CHECK(rig.origin(0).x == Approx(0.0F));
    CHECK(rig.origin(2).x == Approx(6.0F));
    CHECK(rig.origin(1).x == Approx(10.0F));
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.origin(2).x == Approx(5.0F));

    REQUIRE(rig.player.distributeFixtures(rig.all(), Player::Axis::Y).ok());  // C -4..-3, A 0..2, B 3..5
    CHECK(rig.origin(2).y == Approx(-4.0F));
    CHECK(rig.origin(1).y == Approx(3.0F));
    CHECK(rig.origin(0).y == Approx(-1.0F));  // Spanne 9, Höhen 2+2+1, Lücke 2: A liegt bei -1..1
}

TEST_CASE("Region-Node: Rechteck setzen ist ein Undo-Schritt", "[player][region]") {
    Rig rig;
    const auto added = rig.player.addNode("wh.transform.region", Vec2{0.0F, 0.0F});
    REQUIRE(added.ok());
    const NodeId region = added.value();
    const auto value = [&](const char* key) {
        return std::get<double>(rig.player.project()->graph.findNode(region)->parameters.at(key));
    };
    rig.player.closeEditGroup();
    REQUIRE(rig.player.setRegionRect(region, mapping::Region{0.25F, 0.5F, 0.5F, 0.25F}).ok());
    REQUIRE(rig.player.setRegionRect(region, mapping::Region{0.3F, 0.5F, 0.5F, 0.25F}).ok());  // zusammengefasst
    CHECK(value("left") == Approx(0.3));
    CHECK(value("top") == Approx(0.5));
    CHECK(value("width") == Approx(0.5));
    CHECK(value("height") == Approx(0.25));
    REQUIRE(rig.player.undo().ok());  // ein Schritt nimmt alle vier Werte zurück
    CHECK(value("left") == Approx(0.0));
    CHECK(value("width") == Approx(0.5));  // Standardwert

    CHECK(rig.player.setRegionRect(NodeId{9999}, mapping::Region{}).error().code == ErrorCode::NotFound);
    CHECK_FALSE(rig.player.setRegionRect(rig.player.project()->graph.nodes().front().id, mapping::Region{}).ok());
    CHECK_FALSE(rig.player.setRegionRect(region, mapping::Region{std::numeric_limits<float>::quiet_NaN(), 0, 1, 1}).ok());
}

TEST_CASE("Ausgangs-Node: wird zum Ausgang, hängt sich hinter den bisherigen, Rückgängig stellt her", "[player][output]") {
    Rig rig;
    const graph::PortRef before = *rig.player.project()->target;
    const auto added = rig.player.addNode("wh.output.canvas", Vec2{0.0F, 0.0F});
    REQUIRE(added.ok());
    const NodeId output = added.value();

    REQUIRE(rig.player.project()->target.has_value());
    CHECK(*rig.player.project()->target == graph::PortRef{output, "out"});
    const auto* wire = rig.player.project()->graph.findInputConnection(output, "source");
    REQUIRE(wire != nullptr);
    CHECK(wire->from == before);

    REQUIRE(rig.player.setRegionRect(output, mapping::Region{0.0F, 0.0F, 0.5F, 1.0F}).ok());
    REQUIRE(rig.player.undo().ok());  // Rechteck
    REQUIRE(rig.player.undo().ok());  // Node anlegen
    CHECK(*rig.player.project()->target == before);
    CHECK(rig.player.project()->graph.findNode(output) == nullptr);

    // Entfernt man den Ausgangs-Node, geht der Ausgang zurück auf den Node davor.
    const NodeId again = rig.player.addNode("wh.output.canvas", Vec2{0.0F, 0.0F}).value();
    REQUIRE(rig.player.removeNode(again).ok());
    CHECK(*rig.player.project()->target == before);
}

TEST_CASE("Anordnen: ohne Projekt ein Fehler", "[player][arrange]") {
    auto library = nodes::BuiltinLibrary::create().value();
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    Player player{nullptr, *library, gate};
    CHECK_FALSE(player.alignFixtures({mapping::FixtureId{1}}, Player::Align::Left).ok());
    CHECK_FALSE(player.setRegionRect(NodeId{1}, mapping::Region{}).ok());
}

TEST_CASE("Anordnen: eine Ziehbewegung ist ein Schritt, zwei sind zwei", "[player][arrange][undo]") {
    Rig rig;
    for (int i = 0; i < 5; ++i) {
        REQUIRE(rig.player.moveFixtures({rig.id(0)}, Vec3{1.0F, 0.0F, 0.0F}).ok());
    }
    rig.player.closeEditGroup();
    for (int i = 0; i < 3; ++i) {
        REQUIRE(rig.player.moveFixtures({rig.id(0)}, Vec3{0.0F, 1.0F, 0.0F}).ok());
    }
    CHECK(rig.origin(0) == Vec3{5.0F, 3.0F, 0.0F});
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.origin(0) == Vec3{5.0F, 0.0F, 0.0F});
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.origin(0) == Vec3{});
    CHECK_FALSE(rig.player.canUndo());
}

TEST_CASE("Anordnen: Wandbox", "[player][arrange]") {
    Rig rig;
    const mapping::Box wall = rig.player.wallBox();
    CHECK(wall.min.x == Approx(0.0F));
    CHECK(wall.max.x > 11.0F);
}
