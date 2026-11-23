// Belegung der Live-Steuerung (live_bindings.hpp) und DMX-Lernen (dmx_learn.hpp), dazu ihr Zusammenspiel mit dem
// MidiLiveController: eigene MIDI-Belegung statt Profil, DMX-Tasten mit Flanke, DMX-Fader. Entwurf: docs/entwurf-midi.md, 6.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/input/dmx_learn.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/live_bindings.hpp"
#include "whitehole/runtime/midi_live.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
using input::DmxProtocol;
using runtime::LiveFunction;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

TEST_CASE("Belegung: Profil als Start, eigene überschreibt, Textzeilen hin und zurück", "[live][bindings]") {
    const midi::LiveProfile profile = midi::builtinLiveProfiles().front();
    runtime::LiveBindings bindings;
    CHECK(runtime::effectiveMidi(bindings, profile, LiveFunction::Go) == runtime::MidiBinding{0, 82});
    CHECK(runtime::effectiveMidi(bindings, profile, LiveFunction::Crossfade) == runtime::MidiBinding{0, 49});
    bindings.midiOf(LiveFunction::Go) = runtime::MidiBinding{3, 12};
    bindings.midiOf(LiveFunction::Stop) = runtime::MidiBinding{0, -1};  // ausdrücklich aus
    bindings.dmxOf(LiveFunction::Crossfade) = input::DmxAddress{DmxProtocol::Sacn, 7, 512};
    bindings.dmxOf(LiveFunction::Master) = input::DmxAddress{DmxProtocol::ArtNet, 1, 1};  // wird nicht geschrieben
    CHECK(runtime::effectiveMidi(bindings, profile, LiveFunction::Go) == runtime::MidiBinding{3, 12});

    const std::vector<std::string> lines = runtime::serializeBindings(bindings);
    CHECK(lines == std::vector<std::string>{"go midi 3 12", "stop midi 0 -1", "crossfade dmx sacn 7 512"});
    runtime::LiveBindings back = runtime::parseBindings(lines);
    bindings.dmxOf(LiveFunction::Master).reset();
    CHECK(back == bindings);
    const runtime::LiveBindings junk =
        runtime::parseBindings({"", "go midi 16 1", "go midi 0 128", "fade dmx dmx 1 1", "fade dmx artnet 1 0",
                                "master dmx artnet 1 1", "nix midi 0 1", "go midi 0 1 extra"});
    CHECK(junk == runtime::LiveBindings{});
}

TEST_CASE("DMX-Lernen: der am stärksten bewegte Kanal seit dem Scharfstellen", "[dmx][learn]") {
    auto store = std::make_shared<input::DmxStore>();
    std::vector<std::uint8_t> frame(512, 0);
    frame[9] = 200;  // steht schon vorher oben: zählt nicht
    store->write(DmxProtocol::ArtNet, 1, frame);
    input::DmxLearn learn{store};
    CHECK_FALSE(learn.poll().has_value());
    learn.arm();
    CHECK_FALSE(learn.poll().has_value());
    frame[3] = 5;  // Rauschen unter der Schwelle
    store->write(DmxProtocol::ArtNet, 1, frame);
    CHECK_FALSE(learn.poll().has_value());
    frame[3] = 40;
    frame[20] = 90;  // größere Bewegung
    store->write(DmxProtocol::ArtNet, 1, frame);
    const auto got = learn.poll();
    REQUIRE(got.has_value());
    CHECK(*got == input::DmxAddress{DmxProtocol::ArtNet, 1, 21});
    CHECK_FALSE(learn.armed());

    learn.arm();  // ein neues Universe zählt gegen 0
    store->write(DmxProtocol::Sacn, 4, std::vector<std::uint8_t>{0, 0, 50});
    CHECK(learn.poll() == input::DmxAddress{DmxProtocol::Sacn, 4, 3});
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
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    runtime::MidiLiveController controller{player, library->midi};
    double clock{0.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
        REQUIRE(player.start().ok());
        REQUIRE(player.startLive(0).ok());
        REQUIRE(player.step().ok());
        controller.setDmxStore(library->dmx);
    }
    runtime::MidiLiveResult frame(runtime::MidiLiveOptions options = {}) {
        const runtime::MidiLiveResult result = controller.step(options);
        REQUIRE(player.step().ok());
        return result;
    }
    void dmx(std::uint8_t value) { library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0, 0, 0, value}); }
};

}  // namespace

TEST_CASE("Live-Steuerung: eigene MIDI-Belegung ersetzt das Profil", "[midi][midi_live][bindings]") {
    Rig rig;
    runtime::LiveBindings bindings;
    bindings.midiOf(LiveFunction::Go) = runtime::MidiBinding{5, 30};
    rig.controller.setBindings(bindings);
    runtime::MidiLiveOptions options;
    options.useGo = true;
    REQUIRE(rig.player.liveArm(1).ok());
    rig.library->midi->apply(midi::noteOn(0, 82, 127));  // alte Profil-Taste: nichts
    rig.frame(options);
    CHECK_FALSE(rig.player.liveStatus().fading);
    rig.library->midi->apply(midi::noteOn(5, 30, 127));
    rig.frame(options);
    CHECK(rig.player.liveStatus().fading);
}

TEST_CASE("Live-Steuerung: DMX-Taste löst bei steigender Flanke aus, nicht beim ersten Wert", "[dmx][midi_live][bindings]") {
    Rig rig;
    runtime::LiveBindings bindings;
    bindings.dmxOf(LiveFunction::Go) = input::DmxAddress{DmxProtocol::ArtNet, 2, 5};
    bindings.dmxOf(LiveFunction::Stop) = input::DmxAddress{DmxProtocol::ArtNet, 2, 4};
    rig.controller.setBindings(bindings);
    REQUIRE(rig.player.liveArm(1).ok());
    rig.dmx(255);  // erster Wert, schon oben: nichts
    rig.frame();
    CHECK_FALSE(rig.player.liveStatus().fading);
    rig.dmx(0);
    rig.frame();
    rig.dmx(200);
    rig.frame();
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 1);
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0, 0, 255, 200});
    CHECK(rig.frame().leaveRequested);
}

TEST_CASE("Live-Steuerung: DMX-Fader für Überblendzeit und manuelle Überblendung", "[dmx][midi_live][bindings]") {
    Rig rig;
    runtime::LiveBindings bindings;
    bindings.dmxOf(LiveFunction::FadeTime) = input::DmxAddress{DmxProtocol::ArtNet, 2, 1};
    bindings.dmxOf(LiveFunction::Crossfade) = input::DmxAddress{DmxProtocol::ArtNet, 2, 2};
    rig.controller.setBindings(bindings);
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0});
    rig.frame();  // erste Werte
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255, 0});
    rig.frame();
    CHECK(rig.controller.fadeFromFader() == Approx(10.0));

    runtime::MidiLiveOptions options;
    options.useGo = true;
    REQUIRE(rig.player.liveArm(1).ok());
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255, 1});  // unten: Mitnahme
    rig.frame(options);
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255, 128});
    rig.frame(options);
    CHECK(rig.player.liveStatus().manual);
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255, 255});
    rig.frame(options);
    CHECK(rig.player.liveStatus().current == 1);
}

TEST_CASE("Live-Steuerung: veraltete DMX-Werte zählen nicht", "[dmx][midi_live][bindings]") {
    auto now = std::chrono::steady_clock::time_point{} + std::chrono::seconds{10};
    Rig rig;
    auto store = std::make_shared<input::DmxStore>([&now] { return now; });
    rig.controller.setDmxStore(store);
    runtime::LiveBindings bindings;
    bindings.dmxOf(LiveFunction::Go) = input::DmxAddress{DmxProtocol::ArtNet, 2, 1};
    rig.controller.setBindings(bindings);
    REQUIRE(rig.player.liveArm(1).ok());
    store->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0});
    rig.frame();
    now += std::chrono::seconds{5};
    rig.frame();  // zu alt: kein Wert gelesen, Zustand bleibt
    store->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{255});
    rig.frame();
    CHECK(rig.player.liveStatus().fading);
}
