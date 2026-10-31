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

TEST_CASE("Effektbereich: setzen, prüfen, rückgängig, speichern", "[player][region]") {
    Rig rig;
    CHECK_FALSE(rig.player.project()->region.has_value());
    REQUIRE(rig.player.setRegion(mapping::Region{0.0F, 0.0F, 0.5F, 0.5F}).ok());
    REQUIRE(rig.player.project()->region.has_value());
    CHECK(rig.player.project()->region->w == Approx(0.5F));
    CHECK(rig.player.modified());

    CHECK_FALSE(rig.player.setRegion(mapping::Region{0.6F, 0.0F, 0.6F, 1.0F}).ok());  // ragt hinaus
    CHECK_FALSE(rig.player.setRegion(mapping::Region{0.0F, 0.0F, 0.0F, 1.0F}).ok());  // Breite 0
    CHECK(rig.player.project()->region->w == Approx(0.5F));

    REQUIRE(rig.player.setRegion(mapping::Region{0.25F, 0.25F, 0.5F, 0.5F}).ok());  // gleichartig: zusammengefasst
    REQUIRE(rig.player.undo().ok());
    CHECK_FALSE(rig.player.project()->region.has_value());

    REQUIRE(rig.player.redo().ok());
    const auto written = project::writeProject(*rig.player.project());
    REQUIRE(written.ok());
    const std::string& text = written.value();
    CHECK(text.find("region 0.25 0.25 0.5 0.5") != std::string::npos);
    REQUIRE(rig.player.setRegion(std::nullopt).ok());
    CHECK_FALSE(rig.player.project()->region.has_value());
}

TEST_CASE("Effektbereich: ohne Projekt ein Fehler", "[player][region]") {
    auto library = nodes::BuiltinLibrary::create().value();
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    Player player{nullptr, *library, gate};
    CHECK_FALSE(player.setRegion(mapping::Region{}).ok());
    CHECK_FALSE(player.alignFixtures({mapping::FixtureId{1}}, Player::Align::Left).ok());
}
