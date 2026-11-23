// Der Live-Modus auf dem APC mini (MidiLiveController): Pads rufen Looks ab, LEDs zeigen den Zustand, Tasten und
// Fader bedienen GO, Ende, Master, Überblendzeit und manuelle Überblendung. Dazu das Profil (Raster, Textzeile).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>

#include "../midi/fake_midi.hpp"
#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/midi/live_profile.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/midi_live.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

TEST_CASE("Live-Profil: Raster 4 Spalten, erste Zeile oben", "[midi][profile]") {
    const midi::LiveProfile profile = midi::builtinLiveProfiles().front();
    CHECK(profile.columns == 4);
    CHECK(profile.capacity() == 32);
    CHECK(profile.noteForLook(0) == 56);   // links oben
    CHECK(profile.noteForLook(3) == 59);
    CHECK(profile.noteForLook(4) == 48);   // zweite Zeile
    CHECK(profile.noteForLook(31) == 3);   // rechts unten der linken Hälfte
    CHECK_FALSE(profile.noteForLook(32).has_value());
    CHECK_FALSE(profile.noteForLook(-1).has_value());
    CHECK(profile.lookForNote(56) == 0);
    CHECK(profile.lookForNote(3) == 31);
    CHECK_FALSE(profile.lookForNote(60).has_value());  // rechte Hälfte bleibt frei
    CHECK_FALSE(profile.lookForNote(82).has_value());  // Seitentaste
    for (int i = 0; i < 32; ++i) {
        CHECK(profile.lookForNote(*profile.noteForLook(i)) == i);
    }
}

TEST_CASE("Live-Profil: 8 x 8 nutzt alle Pads, keine Tasten", "[midi][profile]") {
    const midi::LiveProfile profile = midi::builtinLiveProfiles().at(1);
    CHECK(profile.capacity() == 64);
    CHECK(profile.noteForLook(7) == 63);
    CHECK(profile.noteForLook(63) == 7);
    CHECK(profile.controls.goNote == -1);
    CHECK(profile.controls.stopNote == -1);
    CHECK(profile.controls.masterCc == 56);
}

TEST_CASE("Live-Profil: Textzeile hin und zurück, kaputte Zeilen abgelehnt", "[midi][profile]") {
    midi::LiveProfile profile = midi::builtinLiveProfiles().front();
    profile.name = "Mein|Profil";
    const std::string line = midi::serializeProfile(profile);
    CHECK(line == "Mein Profil|apcmini1|4|82|83|56|48|49");
    const auto back = midi::parseProfile(line);
    REQUIRE(back.has_value());
    CHECK(back->name == "Mein Profil");
    CHECK(back->columns == 4);
    CHECK(back->controls.crossfadeCc == 49);
    CHECK_FALSE(back->builtin);
    CHECK_FALSE(midi::parseProfile("").has_value());
    CHECK_FALSE(midi::parseProfile("x|apcmini2|4|1|2|3|4|5").has_value());
    CHECK_FALSE(midi::parseProfile("x|apcmini1|9|1|2|3|4|5").has_value());
    CHECK_FALSE(midi::parseProfile("x|apcmini1|4|1|2|3|4|128").has_value());
    CHECK_FALSE(midi::parseProfile("x|apcmini1|4|1|2|3|4").has_value());
    CHECK_FALSE(midi::parseProfile("|apcmini1|4|1|2|3|4|5").has_value());
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
    }
    void live() {
        REQUIRE(player.startLive(0).ok());
        REQUIRE(player.step().ok());
    }
    runtime::MidiLiveResult frame(runtime::MidiLiveOptions options = {}) {
        const runtime::MidiLiveResult result = controller.step(options);
        REQUIRE(player.step().ok());
        return result;
    }
    void pad(int look) { library->midi->apply(midi::noteOn(0, *controller.profile().noteForLook(look), 127)); }
    void fader(int cc, int value) { library->midi->apply(midi::controlChange(0, static_cast<std::uint8_t>(cc), static_cast<std::uint8_t>(value))); }
    /// Letzter gesendeter Wert je Note.
    std::map<int, int> leds() const {
        std::map<int, int> state;
        for (const midi::MidiMessage& m : output.sent) {
            state[m.data1] = m.data2;
        }
        return state;
    }
};

}  // namespace

TEST_CASE("APC live: außerhalb des Live-Betriebs bleiben Pads frei und LEDs unberührt", "[midi][midi_live]") {
    Rig rig;
    rig.pad(1);
    rig.frame();
    CHECK_FALSE(rig.player.liveActive());
    CHECK(rig.output.sent.empty());
}

TEST_CASE("APC live: Live-Start setzt das Raster, Pad blendet über, Ende löscht", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    rig.frame();
    auto leds = rig.leds();
    CHECK(leds[56] == 1);  // Look 1 läuft: grün
    CHECK(leds[57] == 0);
    CHECK(leds[83] == 1);  // Taste "Live beenden" leuchtet
    CHECK(leds[82] == 0);  // GO: nichts vorgeladen
    CHECK(leds[60] == 0);  // rechte Hälfte gelöscht
    const std::size_t sentOnce = rig.output.sent.size();
    rig.frame();
    CHECK(rig.output.sent.size() == sentOnce);  // nichts geändert, nichts gesendet

    rig.pad(1);
    rig.frame();
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 1);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(0.5));  // Zeit des Looks
    rig.frame();  // LEDs nach dem Player-Schritt
    leds = rig.leds();
    CHECK(leds[57] == 5);  // blendet ein: gelb
    CHECK(leds[56] == 1);  // der alte ist noch zu sehen

    rig.clock = 1.0;
    rig.frame();
    rig.frame();
    leds = rig.leds();
    CHECK(leds[57] == 1);
    CHECK(leds[56] == 0);

    rig.player.stopLive();
    rig.frame();
    leds = rig.leds();
    CHECK(leds[57] == 0);
    CHECK(leds[83] == 0);
}

TEST_CASE("APC live: Pads ohne Look und fremde Kanäle tun nichts", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    rig.pad(5);  // nur drei Looks
    rig.library->midi->apply(midi::noteOn(3, 57, 127));  // Kanal 4
    rig.frame();
    CHECK_FALSE(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().armed == -1);
}

TEST_CASE("APC live: Mit GO lädt das Pad vor, GO blinkt und startet", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    runtime::MidiLiveOptions options;
    options.useGo = true;
    rig.pad(2);
    rig.frame(options);
    rig.frame(options);
    CHECK(rig.player.liveStatus().armed == 2);
    auto leds = rig.leds();
    CHECK(leds[58] == 6);  // vorgeladen: gelb blinkend
    CHECK(leds[82] == 2);  // GO blinkt
    rig.library->midi->apply(midi::noteOn(0, 82, 127));
    rig.frame(options);
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 2);
}

TEST_CASE("APC live: Ende-Taste meldet, Fader setzen Master und Überblendzeit", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    rig.library->midi->apply(midi::noteOn(0, 83, 127));
    CHECK(rig.frame().leaveRequested);
    CHECK(rig.player.liveActive());  // ausführen tut es das Studio

    rig.fader(56, 0);
    CHECK(rig.frame().masterChanged);
    CHECK(rig.player.masterBrightness() == Approx(0.0F));

    runtime::MidiLiveOptions options;
    options.fadeOverride = 4.0;
    rig.pad(1);
    rig.frame(options);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(4.0));  // Fader noch nie bewegt: Wert des Fensters
    rig.fader(48, 127);
    rig.pad(2);
    rig.frame(options);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(10.0));
    CHECK(rig.controller.fadeFromFader() == Approx(10.0));
}

TEST_CASE("APC live: manuelle Überblendung mit Mitnahme", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    runtime::MidiLiveOptions options;
    options.useGo = true;
    rig.fader(49, 90);  // steht oben, bevor etwas vorgeladen ist
    rig.frame(options);
    rig.pad(1);
    rig.frame(options);
    REQUIRE(rig.player.liveStatus().armed == 1);
    rig.fader(49, 100);  // noch nicht unten gewesen: keine Wirkung
    rig.frame(options);
    CHECK_FALSE(rig.player.liveStatus().fading);

    rig.fader(49, 0);  // Mitnahme
    rig.fader(49, 64);
    rig.frame(options);
    CHECK(rig.player.liveStatus().manual);
    CHECK(rig.player.liveStatus().progress == Approx(64.0 / 127.0));
    rig.clock = 50.0;  // die Zeit schiebt nicht
    rig.frame(options);
    CHECK(rig.player.liveStatus().progress == Approx(64.0 / 127.0));

    rig.fader(49, 127);
    rig.frame(options);
    CHECK(rig.player.liveStatus().current == 1);
    CHECK_FALSE(rig.player.liveStatus().fading);

    rig.pad(2);  // nächster Look, Fader steht oben
    rig.frame(options);
    rig.fader(49, 120);
    rig.frame(options);
    CHECK_FALSE(rig.player.liveStatus().fading);  // erst wieder nach unten
    rig.fader(49, 2);
    rig.fader(49, 127);
    rig.frame(options);
    CHECK(rig.player.liveStatus().current == 2);
}

TEST_CASE("APC live: neuer Ausgang bekommt alles neu, clear löscht", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    rig.frame();
    midi::testing::FakeMidiOutput second;
    REQUIRE(second.open("Fake Ausgang 20:1").ok());
    rig.controller.setOutput(&second);
    rig.frame();
    CHECK(second.sent.size() > 64);
    rig.controller.clear();
    std::map<int, int> state;
    for (const midi::MidiMessage& m : second.sent) {
        state[m.data1] = m.data2;
    }
    for (const auto& [note, velocity] : state) {
        CHECK(velocity == 0);
    }
}

TEST_CASE("APC live: abgeschaltet verhält er sich wie außerhalb des Live-Betriebs", "[midi][midi_live]") {
    Rig rig;
    rig.live();
    runtime::MidiLiveOptions off;
    off.enabled = false;
    rig.pad(1);
    rig.frame(off);
    CHECK_FALSE(rig.player.liveStatus().fading);
    CHECK(rig.output.sent.empty());
    rig.frame();  // wieder an: Raster wird gesetzt
    CHECK(rig.leds()[56] == 1);
    rig.frame(off);  // aus: gelöscht
    CHECK(rig.leds()[56] == 0);
}
