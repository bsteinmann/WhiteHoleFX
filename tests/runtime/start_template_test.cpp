// Die Vorlage eines neuen Projekts ("Matrix 16x8", ABSTUERZE_TODO.md Punkt 7): Wer im leeren Studio ein Fixture oder eine
// Bauform anlegt, soll in der Bibliothek nur das Eigene sehen. Warum eine eigene Datei: Player::dropSoleFixtureType ist
// die kleine, Qt-freie Hälfte dieser Korrektur; die Oberfläche (nur in der CI testbar) ruft sie nur noch auf.
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kTemplate = R"(whitehole-project 1
settings fps 40

destination 0 127.0.0.1

fixturetype "Matrix 16x8"
  matrix 16 8 pitch 1 1 start top-left first rows serpentine
end

fixture Matrix type "Matrix 16x8"
  patch artnet destination 0 universe 0 channel 1 order RGB gamma 2.2 brightness 1
end

node verlauf wh.generator.gradient
end

node ausgang wh.output.canvas
end
connect verlauf.out -> ausgang.source
output ausgang.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
};

}  // namespace

TEST_CASE("Vorlage: die Bauform des einzigen Fixtures verschwindet, das Fixture behält seine Form", "[player][template]") {
    Rig rig;
    REQUIRE(rig.player.loadText(kTemplate).ok());
    REQUIRE(rig.player.fixtureTypes().size() == 1);
    const std::uint64_t leds = mapping::ledCount(rig.player.project()->layout.fixtures().front().shape);
    REQUIRE(leds == 128);

    REQUIRE(rig.player.dropSoleFixtureType().ok());
    CHECK(rig.player.fixtureTypes().empty());
    REQUIRE(rig.player.project()->layout.fixtures().size() == 1);
    CHECK(rig.player.project()->layout.fixtures().front().type.empty());
    CHECK(mapping::ledCount(rig.player.project()->layout.fixtures().front().shape) == leds);

    // Danach steht in der Bibliothek nur, was der Nutzer selbst anlegt.
    REQUIRE(rig.player.addFixtureType("Mein Streifen", mapping::FixtureShape{mapping::StripShape{30, Vec3{}, Vec3{1.0F, 0.0F, 0.0F}}}).ok());
    REQUIRE(rig.player.fixtureTypes().size() == 1);
    CHECK(rig.player.fixtureTypes().front().name == "Mein Streifen");

    REQUIRE(rig.player.undo().ok());  // ein Schritt Rückgängig je Änderung
}

TEST_CASE("Vorlage: ohne Projekt oder bei mehreren Fixtures geschieht nichts", "[player][template]") {
    Rig rig;
    CHECK(rig.player.dropSoleFixtureType().ok());  // kein Projekt: kein Fehler

    REQUIRE(rig.player.loadText(kTemplate).ok());
    REQUIRE(rig.player.addFixtureOfType("Zweite", "Matrix 16x8", Vec3{0.0F, 20.0F, 0.0F}, Vec3{}, std::nullopt).ok());
    REQUIRE(rig.player.project()->layout.fixtures().size() == 2);
    CHECK(rig.player.dropSoleFixtureType().ok());
    CHECK(rig.player.fixtureTypes().size() == 1);  // zwei Exemplare: die Bauform bleibt
}
