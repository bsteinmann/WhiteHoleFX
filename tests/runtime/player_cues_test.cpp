// Tests der Cuelist im Player (Issue #21): Start, GO, Delay und Fade auf den Live-Decks, Tasten, DMX, Look-Namen.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
using project::Cue;
using project::CueTrigger;
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

Cue cue(const std::string& look, CueTrigger trigger, double delay = 0.0, double fade = 1.0) {
    Cue c;
    c.look = look;
    c.trigger = trigger;
    c.delay = delay;
    c.fade = fade;
    return c;
}

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
    }
    void at(double seconds) {
        clock = seconds;
        REQUIRE(player.step().ok());
    }
};

}  // namespace

TEST_CASE("Player/Cues: setzen prüft Looks, Zeiten und Auslöser", "[player][cues]") {
    Rig rig;
    CHECK(rig.player.cues().empty());
    CHECK(rig.player.setCues({cue("Ruhig", CueTrigger::Go), cue("", CueTrigger::Go)}).ok());
    CHECK(rig.player.cues().size() == 2);
    CHECK(rig.player.modified());

    CHECK(rig.player.setCues({cue("Gibtsnicht", CueTrigger::Go)}).error().code == ErrorCode::NotFound);
    CHECK_FALSE(rig.player.setCues({cue("Ruhig", CueTrigger::Go, 0.0, -1.0)}).ok());
    CHECK_FALSE(rig.player.setCues({cue("Ruhig", CueTrigger::Go, 5000.0)}).ok());
    Cue key = cue("Ruhig", CueTrigger::Key);
    CHECK_FALSE(rig.player.setCues({key}).ok());  // Taste fehlt
    Cue dmx = cue("Ruhig", CueTrigger::Dmx);
    dmx.dmxChannel = 0;
    CHECK_FALSE(rig.player.setCues({dmx}).ok());
    CHECK(rig.player.cues().size() == 2);  // fehlgeschlagene Aufrufe lassen die Liste stehen

    REQUIRE(rig.player.setCues({}).ok());
    CHECK_FALSE(rig.player.cuesStart().ok());  // leere Liste
}

TEST_CASE("Player/Cues: Start zeigt Cue 1, GO blendet mit Delay und Fade zum nächsten", "[player][cues]") {
    Rig rig;
    REQUIRE(rig.player.setCues({cue("Ruhig", CueTrigger::Go), cue("Schnell", CueTrigger::Go, 1.0, 2.0),
                                cue("", CueTrigger::Go, 0.0, 1.0)})
                .ok());
    REQUIRE(rig.player.cuesStart().ok());
    CHECK(rig.player.cuesRunning());
    CHECK(rig.player.liveActive());
    rig.at(0.1);
    CHECK(rig.player.liveStatus().current == 0);  // Look "Ruhig"
    CHECK(rig.player.cueStatus().next == 1);

    rig.at(1.0);
    CHECK_FALSE(rig.player.liveStatus().fading);  // ohne GO passiert nichts
    rig.player.cueGo();
    rig.at(2.0);                                  // ausgelöst; Delay 1 s läuft
    CHECK(rig.player.cueStatus().pending == 1);
    CHECK_FALSE(rig.player.liveStatus().fading);
    rig.at(3.0);                                  // Delay um: Überblendung beginnt
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 1);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(2.0));
    rig.at(5.5);
    CHECK_FALSE(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().current == 1);  // "Schnell" steht

    rig.player.cueGo();                           // Cue 3: Schwarz
    rig.at(6.0);
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == -1);  // das verdeckte Deck ist leer
    rig.at(8.0);
    CHECK_FALSE(rig.player.liveStatus().fading);
    CHECK(rig.player.cueStatus().next == -1);
    CHECK_FALSE(rig.player.cueError().has_value());

    rig.player.cuesStop();
    CHECK_FALSE(rig.player.cuesRunning());
}

TEST_CASE("Player/Cues: Zeit-Auslöser, Taste und DMX", "[player][cues]") {
    Rig rig;
    Cue timed = cue("Schnell", CueTrigger::Time, 0.0, 1.0);
    timed.time = 5.0;
    Cue keyed = cue("Ruhig", CueTrigger::Key, 0.0, 0.0);
    keyed.key = "F2";
    Cue dmx = cue("Schnell", CueTrigger::Dmx, 0.0, 0.0);
    dmx.dmxUniverse = 3;
    dmx.dmxChannel = 2;
    dmx.dmxThreshold = 128;
    REQUIRE(rig.player.setCues({cue("Ruhig", CueTrigger::Go), timed, keyed, dmx}).ok());
    REQUIRE(rig.player.cuesStart().ok());
    rig.at(0.0);
    rig.at(4.9);
    CHECK(rig.player.cueStatus().current == 0);
    rig.at(5.0);  // 0 + 5 s
    CHECK(rig.player.cueStatus().current == 1);
    CHECK(rig.player.liveStatus().fading);

    rig.player.cueKey("f2");  // springt zu Cue 3 (Ruhig), Fade 0
    rig.at(6.0);
    CHECK(rig.player.cueStatus().current == 2);
    CHECK(rig.player.liveStatus().current == 0);
    CHECK_FALSE(rig.player.liveStatus().fading);

    // Cue 4 wartet auf DMX: erst ein niedriger Wert, dann ein Anstieg
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 10});
    rig.at(7.0);
    CHECK(rig.player.cueStatus().current == 2);
    rig.library->dmx->write(input::DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 200});
    rig.at(8.0);
    CHECK(rig.player.cueStatus().current == 3);
    CHECK(rig.player.liveStatus().current == 1);  // "Schnell"
}

TEST_CASE("Player/Cues: Looks umbenennen und löschen halten die Cues lesbar", "[player][cues]") {
    Rig rig;
    REQUIRE(rig.player.setCues({cue("Ruhig", CueTrigger::Go), cue("Schnell", CueTrigger::Go)}).ok());
    REQUIRE(rig.player.renameLook(0, "Sanft").ok());
    CHECK(rig.player.cues()[0].look == "Sanft");
    REQUIRE(rig.player.removeLook(1).ok());  // "Schnell"
    CHECK(rig.player.cues()[1].look.empty()); // wird zu Schwarz

    const auto text = project::writeProject(*rig.player.project());
    REQUIRE(text.ok());
    CHECK(rig.player.loadText(text.value()).ok());  // die Datei bleibt lesbar
    CHECK(rig.player.cues().size() == 2);
}

TEST_CASE("Player/Cues: cueFire springt, ein neues Projekt hält den Ablauf an", "[player][cues]") {
    Rig rig;
    CHECK_FALSE(rig.player.cueFire(0).ok());  // läuft nicht
    REQUIRE(rig.player.setCues({cue("Ruhig", CueTrigger::Go), cue("Schnell", CueTrigger::Go, 0.0, 0.0)}).ok());
    REQUIRE(rig.player.cuesStart().ok());
    CHECK(rig.player.cueFire(7).error().code == ErrorCode::NotFound);
    REQUIRE(rig.player.cueFire(1).ok());
    rig.at(0.5);
    CHECK(rig.player.liveStatus().current == 1);
    REQUIRE(rig.player.loadText(kProject).ok());
    CHECK_FALSE(rig.player.cuesRunning());
}
