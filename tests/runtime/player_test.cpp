// Tests des Players (Zustände der Oberfläche) und des schaltbaren Paketziels.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <thread>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
settings fps 25

fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
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
};

}  // namespace

TEST_CASE("Gated: ausgeschaltet werden Pakete verworfen und gezählt", "[player]") {
    RecordingSink inner;
    runtime::GatedPacketSink gate{inner};
    const std::vector<std::uint8_t> packet(10, 1);
    CHECK_FALSE(gate.enabled());
    CHECK(gate.send({"1.2.3.4", 1}, packet).ok());
    CHECK(inner.count() == 0);
    CHECK(gate.suppressed() == 1);

    gate.setEnabled(true);
    CHECK(gate.send({"1.2.3.4", 1}, packet).ok());
    CHECK(inner.count() == 1);
    CHECK(gate.suppressed() == 1);

    gate.setEnabled(false);
    CHECK(gate.send({"1.2.3.4", 1}, packet).ok());
    CHECK(inner.count() == 1);
    CHECK(gate.suppressed() == 2);
}

TEST_CASE("Gated: Fehler des Ziels werden durchgereicht", "[player]") {
    RecordingSink inner;
    inner.failFor("1.2.3.4");
    runtime::GatedPacketSink gate{inner};
    gate.setEnabled(true);
    CHECK_FALSE(gate.send({"1.2.3.4", 1}, std::vector<std::uint8_t>(3, 0)).ok());
}

TEST_CASE("Player: leer, dann geladen", "[player]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    CHECK_FALSE(player.status().loaded);
    CHECK(player.project() == nullptr);
    CHECK(player.framesPerSecond() == 40.0);
    CHECK_FALSE(player.image({}).ok());
    CHECK_FALSE(player.start().ok());
    CHECK(player.step().ok());  // ohne Lauf ein No-op

    REQUIRE(player.loadText(kProject).ok());
    const runtime::PlayerStatus s = player.status();
    CHECK(s.loaded);
    CHECK_FALSE(s.running);
    CHECK(s.ledCount == 8);
    CHECK(s.framesPerSecond == 25.0);
    CHECK(player.project() != nullptr);

    // Ohne Lauf zeigt das Bild das Layout (nicht schwarz).
    const auto image = player.image({});
    REQUIRE(image.ok());
    std::size_t bright = 0;
    for (std::size_t i = 0; i < image.value().rgba.size(); i += 4) {
        if (image.value().rgba[i] > 100 || image.value().rgba[i + 1] > 100 || image.value().rgba[i + 2] > 100) {
            ++bright;
        }
    }
    CHECK(bright > 200);
}

TEST_CASE("Player: ein fehlerhaftes load lässt das bisherige Projekt stehen", "[player]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kProject).ok());

    const auto bad = player.loadText("whitehole-project 1\nblubb\n");
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().message.find("Zeile 2") != std::string::npos);
    CHECK(player.status().ledCount == 8);

    // Auch ein Ausgabefehler (Ziel unbekannt) fällt beim Laden auf und ändert nichts.
    const auto badPatch = player.loadText(
        "whitehole-project 1\nfixture A\n  matrix 2 2\n  patch artnet destination 3\nend\n");
    CHECK_FALSE(badPatch.ok());
    CHECK(player.status().ledCount == 8);

    CHECK_FALSE(player.load("/nicht/vorhanden.whfx").ok());
    CHECK(player.status().loaded);
}

TEST_CASE("Player: Start, Frames, Stopp", "[player]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kProject).ok());
    REQUIRE(player.start().ok());
    CHECK(player.status().running);
    CHECK(player.start().ok());  // zweiter Start ist harmlos

    for (int i = 0; i < 4; ++i) {
        REQUIRE(player.step().ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    // Das Fake liefert Readbacks beim nächsten poll(); ein weiterer Frame holt sie ab.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (player.status().packets + rig.gate.suppressed() == 0 && std::chrono::steady_clock::now() < deadline) {
        REQUIRE(player.step().ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    const runtime::PlayerStatus s = player.status();
    CHECK(s.running);
    CHECK(s.frames >= 4);
    // Das Gate ist aus: Es kommt nichts am Ziel an, aber die Pipeline hat gesendet.
    CHECK(rig.recording.count() == 0);
    CHECK(rig.gate.suppressed() > 0);
    CHECK(s.sendErrors == 0);

    // Während des Laufs zeigt das Bild die Farben des Effekts (hier: Nullen, also dunkle LEDs).
    CHECK(player.image({}).ok());

    player.stop();
    CHECK_FALSE(player.status().running);
    CHECK(player.status().loaded);
    CHECK(player.status().frames == 0);
}

TEST_CASE("Player: mit eingeschaltetem Gate kommen Pakete an", "[player]") {
    Rig rig;
    rig.gate.setEnabled(true);
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kProject).ok());
    REQUIRE(player.start().ok());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (rig.recording.count() == 0 && std::chrono::steady_clock::now() < deadline) {
        REQUIRE(player.step().ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    CHECK(rig.recording.count() > 0);
    CHECK(rig.gate.suppressed() == 0);
}

TEST_CASE("Player: ohne Gerät läuft alles außer dem Start", "[player]") {
    Rig rig;
    runtime::Player player{nullptr, *rig.library, rig.gate};
    REQUIRE(player.loadText(kProject).ok());
    const auto r = player.start();
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("GPU") != std::string::npos);
    CHECK(player.image({}).ok());
}

TEST_CASE("Player: ein Projekt ohne output-Zeile lässt sich laden, aber nicht starten", "[player]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(
                "whitehole-project 1\ndestination 0 10.0.0.5\nfixture A\n  matrix 2 2\n  patch artnet destination 0\nend\n")
                .ok());
    const auto r = player.start();
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("output") != std::string::npos);
    CHECK_FALSE(player.status().running);
}

TEST_CASE("Player: neu laden während des Laufs hält an", "[player]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kProject).ok());
    REQUIRE(player.start().ok());
    REQUIRE(player.loadText(kProject).ok());
    CHECK_FALSE(player.status().running);
    REQUIRE(player.start().ok());
    CHECK(player.status().running);
}

TEST_CASE("Player: ein Fehler beim Einreichen stoppt den Lauf und bleibt sichtbar", "[player]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kProject).ok());
    REQUIRE(player.start().ok());
    rig.device.failNextSubmit();
    CHECK_FALSE(player.step().ok());
    const runtime::PlayerStatus s = player.status();
    CHECK_FALSE(s.running);
    REQUIRE(s.lastError.has_value());
    CHECK(s.lastError->message.find("submit") != std::string::npos);
}
