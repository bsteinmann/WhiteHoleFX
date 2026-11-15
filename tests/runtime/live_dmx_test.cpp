// Tests der Look-Auswahl im Live-Betrieb per DMX (DMX-Fernsteuerung für Live-Modus, Teil 1): die reine
// Rechnung (lookIndexFromDmxValue) und ihr Zusammenspiel mit dem Player (Kanalwert, Zeitüberschreitung,
// Umschalt- statt Dauer-Verhalten, Überblendzeit-Kanal).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::input::DmxProtocol;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

TEST_CASE("lookIndexFromDmxValue: 0 und keine Looks heißen 'kein Wechsel'", "[live_dmx]") {
    CHECK_FALSE(runtime::lookIndexFromDmxValue(0, 3).has_value());
    CHECK_FALSE(runtime::lookIndexFromDmxValue(200, 0).has_value());
}

TEST_CASE("lookIndexFromDmxValue: ein Look nimmt jeden Wert ab 1", "[live_dmx]") {
    CHECK(runtime::lookIndexFromDmxValue(1, 1) == 0);
    CHECK(runtime::lookIndexFromDmxValue(255, 1) == 0);
}

TEST_CASE("lookIndexFromDmxValue: gleich große Stufen über die Look-Zahl verteilt", "[live_dmx]") {
    // 3 Looks: 255 Werte in Stufen von 85 (aufgerundet): 1..85 -> 0, 86..170 -> 1, 171..255 -> 2
    CHECK(runtime::lookIndexFromDmxValue(1, 3) == 0);
    CHECK(runtime::lookIndexFromDmxValue(85, 3) == 0);
    CHECK(runtime::lookIndexFromDmxValue(86, 3) == 1);
    CHECK(runtime::lookIndexFromDmxValue(170, 3) == 1);
    CHECK(runtime::lookIndexFromDmxValue(171, 3) == 2);
    CHECK(runtime::lookIndexFromDmxValue(255, 3) == 2);
}

TEST_CASE("lookIndexFromDmxValue: bleibt innerhalb des Bereichs, auch wenn die letzte Stufe kleiner ist", "[live_dmx]") {
    // 7 Looks: Stufe 37 (aufgerundet von 255/7); die letzte Stufe ist kürzer, darf aber nicht über Index 6 gehen
    CHECK(runtime::lookIndexFromDmxValue(255, 7) == 6);
}

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
end
output plasma.out

look Ruhig fade 3
node n1 wh.generator.plasma
end
output n1.out
endlook

look Schnell fade 0.5
node a wh.generator.noise
end
output a.out
endlook
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    double clock{0.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
        REQUIRE(player.start().ok());
        REQUIRE(player.startLive(0).ok());
        REQUIRE(player.step().ok());  // Live-Graph aufgebaut
    }
};

}  // namespace

TEST_CASE("Live per DMX: der Kanalwert wählt den Look und löst die Überblendung aus", "[player][live_dmx]") {
    Rig rig;
    rig.player.setLiveDmx(runtime::LiveDmxSource{DmxProtocol::ArtNet, 2, 5, 0});
    CHECK_FALSE(rig.player.liveFromDmx());  // noch kein Paket

    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0, 0, 0, 200});  // Kanal 5 -> Look 1
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveFromDmx());
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 1);
    // Ohne Fade-Kanal gilt die im Look gespeicherte Zeit ("Schnell": 0,5 s)
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(0.5));
}

TEST_CASE("Live per DMX: derselbe Wert löst nicht jeden Frame erneut aus", "[player][live_dmx]") {
    Rig rig;
    rig.player.setLiveDmx(runtime::LiveDmxSource{DmxProtocol::ArtNet, 2, 1, 0});
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{200});  // -> Look 1, eigene Zeit (0,5 s)
    REQUIRE(rig.player.step().ok());  // löst aus: die Überblendung beginnt bei Zeitpunkt 0 (clock == 0.0)
    REQUIRE(rig.player.liveStatus().fading);

    rig.clock = 0.25;  // halbe Fade-Zeit (0,5 s gesamt)
    REQUIRE(rig.player.step().ok());
    const double progressAtHalf = rig.player.liveStatus().progress;
    CHECK(progressAtHalf > 0.0);
    CHECK(progressAtHalf < 1.0);
    // Derselbe Kanalwert noch einmal (kein neuer Wechsel): Wäre erneut ausgelöst worden, stünde die
    // Überblendung wieder bei 0 statt weiterzulaufen.
    rig.clock = 0.4;
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveStatus().progress > progressAtHalf);
}

TEST_CASE("Live per DMX: 0 heißt kein Wechsel, der Fade-Kanal überschreibt die Zeit des Looks", "[player][live_dmx]") {
    Rig rig;
    rig.player.setLiveDmx(runtime::LiveDmxSource{DmxProtocol::ArtNet, 2, 1, 2});
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 255});  // Auswahl 0: kein Wechsel
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveStatus().current == 0);
    CHECK_FALSE(rig.player.liveStatus().fading);

    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255, 255});  // Auswahl -> Look 1, Fade voll
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveStatus().incoming == 1);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(runtime::kLiveDmxMaxFadeSeconds));
}

TEST_CASE("Live per DMX: Abschalten, ungültiger Kanal, veraltetes Signal hält den Stand", "[player][live_dmx]") {
    Rig rig;
    rig.player.setLiveDmx(runtime::LiveDmxSource{DmxProtocol::ArtNet, 2, 1, 0});
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{200});
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveFromDmx());
    CHECK(rig.player.liveStatus().incoming == 1);

    rig.player.setLiveDmx(std::nullopt);
    CHECK_FALSE(rig.player.liveFromDmx());
    CHECK_FALSE(rig.player.liveDmx().has_value());

    // Kein Projekt-Universe beschrieben (falsches Protokoll): kein frisches Signal
    rig.player.setLiveDmx(runtime::LiveDmxSource{DmxProtocol::Sacn, 9, 1, 0});
    REQUIRE(rig.player.step().ok());
    CHECK_FALSE(rig.player.liveFromDmx());
}
