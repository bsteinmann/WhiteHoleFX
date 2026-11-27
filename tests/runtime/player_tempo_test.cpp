// Gemeinsame Tempo-Uhr am Player: der Player schiebt sie mit seiner Live-Uhr weiter, das Tempo überlebt Look-Wechsel,
// getappt wird über Player::tempoTap oder eine MIDI/DMX-Belegung "tap". Wunsch vom 27.11. (CLAUDE.md, Tempo-Uhr).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/input/dmx_store.hpp"
#include "whitehole/midi/midi_message.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/live_bindings.hpp"
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

look Eins fade 0
node a wh.generator.plasma
end
node t wh.value.tempo
end
connect t.phase -> a.scale
output a.out
endlook

look Zwei fade 0
node b wh.generator.noise
end
output b.out
endlook
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    double clock{100.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
        REQUIRE(player.start().ok());
        REQUIRE(player.step().ok());
    }
    void run(double seconds) {
        clock += seconds;
        REQUIRE(player.step().ok());
    }
};

}  // namespace

TEST_CASE("Player-Tempo: die Uhr läuft mit jedem Bild und überlebt Look-Wechsel", "[runtime][tempo]") {
    Rig rig;
    rig.player.setTempoFixedBpm(60.0);
    rig.player.setTempoSource(values::TempoClock::Source::Fixed);
    rig.run(0.5);
    CHECK(rig.player.tempo().bpm == Approx(60.0));
    CHECK(rig.player.tempo().phase == Approx(0.5));

    REQUIRE(rig.player.startLive(0).ok());
    rig.run(0.25);
    REQUIRE(rig.player.liveTrigger(1).ok());
    rig.run(0.5);
    REQUIRE(rig.player.liveTrigger(0).ok());  // zurück: der Look mit dem Tempo-Node wird neu geladen
    rig.run(0.0);
    CHECK(rig.player.tempo().beat == 1);  // weitergezählt statt bei 0 neu
    CHECK(rig.player.tempo().phase == Approx(0.25));

    // Ein anderes Projekt: die Uhr läuft weiter
    rig.player.stopLive();
    REQUIRE(rig.player.loadText(kProject).ok());
    if (!rig.player.status().running) {
        REQUIRE(rig.player.start().ok());
    }
    rig.run(0.5);
    CHECK(rig.player.tempo().phase == Approx(0.75));
}

TEST_CASE("Player-Tempo: Tap über den Player und über eine MIDI-Belegung", "[runtime][tempo]") {
    Rig rig;
    rig.run(0.1);
    rig.player.tempoTap();
    rig.run(0.0);
    rig.run(0.5);
    rig.player.tempoTap();
    rig.run(0.0);
    CHECK(rig.player.tempo().bpm == Approx(120.0));

    runtime::MidiLiveController controller{rig.player, rig.library->midi};
    runtime::LiveBindings bindings;
    bindings.midiOf(runtime::LiveFunction::Tap) = runtime::MidiBinding{0, 60};
    controller.setBindings(bindings);
    for (int i = 0; i < 5; ++i) {  // die Uhr mittelt die letzten vier Abstände
        rig.run(0.4);
        rig.library->midi->apply(midi::noteOn(0, 60, 100));
        (void)controller.step({});  // außerhalb des Live-Betriebs: der Tap zählt trotzdem
        rig.run(0.0);
    }
    CHECK(rig.player.tempo().bpm == Approx(150.0));
}

TEST_CASE("Player-Tempo: Quelle Audio übernimmt Schläge und Tempo des Beat-Erkenners", "[runtime][tempo]") {
    Rig rig;
    rig.player.setTempoSource(values::TempoClock::Source::Audio);
    audio::AudioFrame frame;
    frame.beat.count = 5;  // erster Blick: alte Schläge werden nicht nachgeholt
    frame.beat.bpm = 100.0F;
    rig.library->audio->write(frame);
    rig.run(0.1);
    CHECK(rig.player.tempo().bpm == Approx(120.0));
    frame.beat.count = 6;
    rig.library->audio->write(frame);
    rig.run(0.1);
    CHECK(rig.player.tempo().bpm == Approx(100.0));
    CHECK(rig.player.tempo().sinceBeat == Approx(0.0));
}

TEST_CASE("Player-Tempo: Quelle DMX liest das Tempo aus einem Kanal", "[runtime][tempo]") {
    Rig rig;
    rig.player.setTempoSource(values::TempoClock::Source::Dmx);
    rig.player.setTempoDmx(input::DmxAddress{input::DmxProtocol::ArtNet, 3, 2});
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 140});
    rig.run(0.1);
    CHECK(rig.player.tempo().bpm == Approx(140.0));
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 0});  // 0 = unverändert
    rig.run(0.1);
    CHECK(rig.player.tempo().bpm == Approx(140.0));
    rig.player.setTempoDmx(std::nullopt);
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 90});
    rig.run(0.1);
    CHECK(rig.player.tempo().bpm == Approx(140.0));
}
