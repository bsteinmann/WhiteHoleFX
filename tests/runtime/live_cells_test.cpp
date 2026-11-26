// Felder des Live-Rasters auslösen: derselbe Look als Umschalten und als Flash, MIDI-Pads auf Feldern, DMX-Auswahl.
// Entwurf: docs/entwurf-live-raster.md.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>

#include "../midi/fake_midi.hpp"
#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/midi/live_profile.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/midi_live.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
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
output plasma.out

look Eins fade 3
node a wh.generator.plasma
end
output a.out
endlook

look Zwei fade 0.5
node b wh.generator.noise
end
output b.out
endlook

look Drei fade 1
node c wh.generator.plasma
end
output c.out
endlook

livegrid 4 2
cell 1 1 look Eins
cell 1 2 look Zwei
cell 1 3 look Zwei flash in 0.25 return 4
cell 2 2 look Drei
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    midi::testing::FakeMidiOutput output;
    runtime::MidiLiveController controller{player, library->midi};
    double clock{0.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
        REQUIRE(player.start().ok());
        REQUIRE(output.open("Fake Ausgang 20:1").ok());
        controller.setOutput(&output);
        REQUIRE(player.startLive(0).ok());
        REQUIRE(player.step().ok());
    }
    void run(double seconds) {
        clock += seconds;
        REQUIRE(player.step().ok());
    }
    runtime::MidiLiveResult frame() {
        const runtime::MidiLiveResult result = controller.step({});
        REQUIRE(player.step().ok());
        return result;
    }
    [[nodiscard]] int shown() const { return player.liveStatus().fading ? player.liveStatus().incoming : player.liveStatus().current; }
    /// Pad für Position (Zeile, Spalte) im Raster des Profils (4 Spalten).
    void pad(int row, int column, bool down = true) {
        const auto note = controller.profile().noteForLook(row * controller.profile().columns + column);
        REQUIRE(note.has_value());
        library->midi->apply(down ? midi::noteOn(0, *note, 127) : midi::noteOff(0, *note));
    }
    std::map<int, int> leds() const {
        std::map<int, int> state;
        for (const midi::MidiMessage& m : output.sent) {
            state[m.data1] = m.data2;
        }
        return state;
    }
};

}  // namespace

TEST_CASE("Felder: derselbe Look als Umschalten und als Flash", "[live][livegrid]") {
    Rig rig;
    CHECK(rig.player.cellLook(1) == 1);
    CHECK(rig.player.cellLook(2) == 1);
    CHECK(rig.player.cellLook(3) == -1);  // leeres Feld

    REQUIRE(rig.player.liveCellDown(3).ok());  // leer: tut nichts
    CHECK(rig.shown() == 0);
    CHECK_FALSE(rig.player.liveCellDown(99).ok());

    // Feld 3 (Zwei als Flash): Einblendzeit 0.25 s, Rückblende 4 s zu Eins
    REQUIRE(rig.player.liveCellDown(2).ok());
    CHECK(rig.shown() == 1);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(0.25));
    REQUIRE(rig.player.liveCellUp(2).ok());
    CHECK(rig.player.liveStatus().incoming == 0);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(4.0));
    rig.run(5.0);
    CHECK(rig.shown() == 0);

    // Feld 2 (Zwei zum Umschalten): bleibt stehen, Loslassen tut nichts, Zeit des Looks
    REQUIRE(rig.player.liveCellDown(1).ok());
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(0.5));
    REQUIRE(rig.player.liveCellUp(1).ok());
    rig.run(1.0);
    CHECK(rig.shown() == 1);
    CHECK(rig.player.liveFlashHeld().empty());
}

TEST_CASE("Felder: Auswahl per liveCellTrigger schaltet auch ein Flash-Feld um", "[live][livegrid]") {
    Rig rig;
    REQUIRE(rig.player.liveCellTrigger(2).ok());
    rig.run(1.0);
    CHECK(rig.shown() == 1);
    CHECK(rig.player.liveFlashHeld().empty());
    REQUIRE(rig.player.liveCellTrigger(3).ok());  // leeres Feld: nichts
    rig.run(1.0);
    CHECK(rig.shown() == 1);
}

TEST_CASE("Felder: MIDI-Pads lösen Felder aus, leere Felder bleiben dunkel", "[live][livegrid][midi_live]") {
    Rig rig;
    rig.frame();
    auto leds = rig.leds();
    CHECK(leds[56] == 1);  // Feld 1/1 (Eins) läuft: grün
    CHECK(leds[57] == 5);  // Feld 1/2 (Zwei, umschalten): gelb
    CHECK(leds[58] == 3);  // Feld 1/3 (Zwei, flash): rot
    CHECK(leds[59] == 0);  // Feld 1/4 leer
    CHECK(leds[48] == 0);  // Feld 2/1 leer
    CHECK(leds[49] == 5);  // Feld 2/2 (Drei)

    rig.pad(0, 2);  // Flash-Feld halten
    rig.frame();
    CHECK(rig.shown() == 1);
    rig.frame();
    leds = rig.leds();
    CHECK(leds[58] == 1);  // nur das gedrückte Feld leuchtet, nicht das andere mit demselben Look
    CHECK(leds[57] == 5);
    CHECK(rig.player.liveStatus().incomingCell == 2);
    rig.pad(0, 2, false);  // loslassen: zurück zu Eins
    rig.frame();
    CHECK(rig.player.liveStatus().incoming == 0);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(4.0));

    rig.pad(1, 3);  // leeres Feld: nichts
    rig.frame();
    rig.pad(1, 1);  // Drei umschalten
    rig.frame();
    CHECK(rig.player.liveStatus().incoming == 2);
}

TEST_CASE("Felder: der DMX-Auswahlkanal zählt Felder, nicht Looks", "[live][livegrid][live_dmx]") {
    Rig rig;
    // 4 x 2 = 8 Felder: Stufen von 32 (aufgerundet von 255/8); Wert 40 -> Feld 1 (Zwei, umschalten)
    rig.player.setLiveDmx(runtime::LiveDmxSource{input::DmxProtocol::ArtNet, 2, 1, 0});
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{40});
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 1);

    rig.clock = 2.0;
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255});  // Feld 7: leer, nichts
    REQUIRE(rig.player.step().ok());
    CHECK(rig.shown() == 1);
    CHECK(rig.player.liveFlashHeld().empty());

    rig.library->dmx->write(input::DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{72});  // Feld 2 (Flash-Feld) schaltet um
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveFlashHeld().empty());
}

TEST_CASE("Felder: nur das abgerufene Feld gilt als aktiv, auch bei demselben Look", "[live][livegrid]") {
    Rig rig;
    CHECK(rig.player.liveStatus().currentCell == 0);  // Start: erstes Feld mit dem Startlook
    REQUIRE(rig.player.liveCellDown(1, std::nullopt, true).ok());  // Zwei über Feld 1 vorladen
    CHECK(rig.player.liveStatus().armedCell == 1);
    REQUIRE(rig.player.liveGo(0.0).ok());
    rig.run(0.1);
    CHECK(rig.player.liveStatus().current == 1);
    CHECK(rig.player.liveStatus().currentCell == 1);  // nicht Feld 2, obwohl dort derselbe Look liegt
    // Flash über Feld 2 und zurück: kehrt zu Feld 1 zurück
    REQUIRE(rig.player.liveCellDown(2).ok());
    CHECK(rig.player.liveStatus().incomingCell == 2);
    REQUIRE(rig.player.liveCellUp(2).ok());
    CHECK(rig.player.liveStatus().incomingCell == 1);
    // Abruf ohne Feld (Look-Index): kein Feld bekannt
    REQUIRE(rig.player.liveTrigger(2, 0.0).ok());
    rig.run(0.1);
    CHECK(rig.player.liveStatus().currentCell == -1);
}

TEST_CASE("Hintergrund-Übersetzen: die Überblendung wartet, bis der neue Shader fertig ist", "[live][background]") {
    Rig rig;
    rig.device.holdCompiles(true);
    REQUIRE(rig.player.liveTrigger(2, 1.0).ok());  // Drei, 1 s
    rig.run(0.0);
    rig.run(0.5);  // Treiber übersetzt noch: die Ausgabe zeigt den bisherigen Stand, der Fade steht
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().progress == Approx(0.0));
    rig.device.finishCompiles();
    rig.run(0.0);  // umgeschaltet
    rig.run(0.5);
    CHECK(rig.player.liveStatus().progress == Approx(0.5));
}

TEST_CASE("Hintergrund-Übersetzen: Flash-Felder sind vorab übersetzt, Drücken und Loslassen übersetzen nicht", "[live][background]") {
    // Rückmeldung vom 26.11.: Flash-Looks stockten bei jedem Drücken und Loslassen. Der Shader-Text hing an den
    // Node-Nummern, die jedes Vorladen neu vergibt (der Zwischenspeicher traf nie), und das erste Drücken einer neuen
    // Kombination musste warten. Jetzt bestellt der Player die Felder des laufenden Looks vorab, ein Feld je Frame.
    Rig rig;
    for (int i = 0; i < 4; ++i) {
        rig.run(0.05);
    }
    // Zwei (Flash-Feld zuerst), dann Eins; Drei ist wie Eins ein Plasma mit Standardwerten, also derselbe Shader
    CHECK(rig.device.prefetchesBegun() == 2);
    const int start = rig.device.compilesBegun();
    REQUIRE(rig.player.liveCellDown(2).ok());  // Flash "Zwei" über "Eins"
    rig.run(0.0);
    CHECK(rig.shown() == 1);
    CHECK(rig.device.compilesBegun() == start);  // kam aus dem Zwischenspeicher
    REQUIRE(rig.player.liveCellUp(2).ok());     // zurück zu "Eins": dieselbe Struktur wie gerade
    rig.run(0.0);
    rig.run(5.0);
    CHECK(rig.shown() == 0);
    REQUIRE(rig.player.liveCellDown(2).ok());  // zweites Mal
    rig.run(0.0);
    CHECK(rig.shown() == 1);
    REQUIRE(rig.player.liveCellUp(2).ok());
    rig.run(0.0);
    rig.run(5.0);
    CHECK(rig.shown() == 0);
    CHECK(rig.device.compilesBegun() == start);
}

TEST_CASE("Hintergrund-Übersetzen: abgerufen, bevor die Vorab-Übersetzung fertig ist, wird sie vorgezogen", "[live][background]") {
    Rig rig;  // der Aufbau-Frame hat das Flash-Feld (Zwei) schon vorab übersetzt
    rig.device.holdCompiles(true);
    rig.run(0.05);  // bestellt Eins (über Eins), der Treiber arbeitet noch
    REQUIRE(rig.device.prefetchesBegun() == 2);
    const int start = rig.device.compilesBegun();
    REQUIRE(rig.player.liveCellTrigger(0, 1.0).ok());  // Eins noch einmal abrufen: genau diese Kombination
    rig.run(0.0);
    CHECK(rig.device.promotions() == 1);
    CHECK(rig.device.compilesBegun() == start);  // nicht ein zweites Mal übersetzt
    rig.device.finishCompiles();
    rig.run(0.0);
    rig.run(0.5);
    CHECK(rig.player.liveStatus().progress == Approx(0.5));
}

TEST_CASE("Hintergrund-Übersetzen: ein anderer Look verwirft die offenen Vorab-Aufträge", "[live][background]") {
    Rig rig;
    rig.device.holdCompiles(true);
    rig.run(0.05);  // Eins bestellt, offen
    const int cancelled = rig.device.compilesCancelled();
    REQUIRE(rig.player.liveCellTrigger(1, 0.0).ok());  // Zwei sofort (vorab übersetzt: aus dem Zwischenspeicher)
    rig.run(0.0);
    CHECK(rig.player.liveStatus().current == 1);
    CHECK(rig.device.compilesCancelled() == cancelled + 1);  // der Auftrag für "über Eins" verfällt
}
