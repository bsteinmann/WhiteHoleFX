// Tests des Players (Zustände der Oberfläche) und des schaltbaren Paketziels.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
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
    CHECK_FALSE(player.image3d({}, {}).ok());
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

    // Dieselben Farben durch die 3D-Kamera: gleiche Größe, und Drehen ändert das Bild.
    const auto front = player.image3d(preview::cameraFor(preview::ViewPreset::Front), {});
    const auto turned = player.image3d(preview::cameraFor(preview::ViewPreset::Perspective), {});
    REQUIRE(front.ok());
    REQUIRE(turned.ok());
    CHECK(front.value().width == image.value().width);
    CHECK(front.value().rgba != turned.value().rgba);
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

namespace {

const char* const kParamProject = R"(whitehole-project 1
destination 0 10.0.0.5

fixture Panel
  matrix 4 2
  patch artnet destination 0
end

node t wh.value.time
end
node a wh.generator.gradient
end
node b wh.generator.gradient
end
node s wh.transform.scale
end
node r wh.transform.rotate
end
connect a.out -> s.source
connect s.out -> r.source
connect t.seconds -> r.angle
output r.out
)";

const runtime::NodeInfo* findNode(const std::vector<runtime::NodeInfo>& nodes, const std::string& title) {
    for (const auto& n : nodes) {
        if (n.title == title) {
            return &n;
        }
    }
    return nullptr;
}

const runtime::ParameterInfo* findParameter(const runtime::NodeInfo& node, const std::string& key) {
    for (const auto& p : node.parameters) {
        if (p.key == key) {
            return &p;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Player: nodes() beschreibt Parameter, Bereiche und verbundene Eingänge", "[player][parameters]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    CHECK(player.nodes().empty());
    REQUIRE(player.loadText(kParamProject).ok());
    const auto nodes = player.nodes();

    // Gleiche Typen werden durchnummeriert, einzelne nicht.
    REQUIRE(findNode(nodes, "Linear Gradient 1") != nullptr);
    REQUIRE(findNode(nodes, "Linear Gradient 2") != nullptr);
    REQUIRE(findNode(nodes, "Scale") != nullptr);
    REQUIRE(findNode(nodes, "Rotate") != nullptr);
    REQUIRE(findNode(nodes, "Time") != nullptr);

    const auto* scale = findParameter(*findNode(nodes, "Scale"), "factor");
    REQUIRE(scale != nullptr);
    CHECK(std::get<double>(scale->value) == 1.0);
    CHECK(scale->minValue == 0.01);
    CHECK(scale->maxValue == 100.0);
    CHECK_FALSE(scale->driven);

    // Der Drehwinkel hängt an der Zeit: ein Regler hätte keine Wirkung.
    const auto* angle = findParameter(*findNode(nodes, "Rotate"), "angle");
    REQUIRE(angle != nullptr);
    CHECK(angle->driven);

    const auto* colorA = findParameter(*findNode(nodes, "Linear Gradient 1"), "colorA");
    REQUIRE(colorA != nullptr);
    CHECK(std::holds_alternative<ColorRgba>(colorA->value));
}

TEST_CASE("Player: setParameter ändert den Wert, begrenzt ihn und meldet Fehler", "[player][parameters]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    CHECK_FALSE(player.setParameter(NodeId{1}, "factor", 2.0).ok());  // kein Projekt
    REQUIRE(player.loadText(kParamProject).ok());
    const NodeId scale = findNode(player.nodes(), "Scale")->id;

    auto r = player.setParameter(scale, "factor", 2.5);
    REQUIRE(r.ok());
    CHECK(std::get<double>(r.value()) == 2.5);
    CHECK(std::get<double>(findParameter(*findNode(player.nodes(), "Scale"), "factor")->value) == 2.5);

    r = player.setParameter(scale, "factor", 5000.0);
    REQUIRE(r.ok());
    CHECK(std::get<double>(r.value()) == 100.0);  // begrenzt

    CHECK(player.setParameter(scale, "factor", std::string{"x"}).error().code == ErrorCode::InvalidArgument);
    CHECK(player.setParameter(scale, "nix", 1.0).error().code == ErrorCode::NotFound);
    CHECK(player.setParameter(NodeId{999}, "factor", 1.0).error().code == ErrorCode::NotFound);
    CHECK(player.setParameter(scale, "factor", std::nan("")).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("Player: Parameteränderungen im Lauf bauen keine neue Pipeline", "[player][parameters]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kParamProject).ok());
    REQUIRE(player.start().ok());
    REQUIRE(player.step().ok());
    const std::size_t pipelines = rig.device.pipelineDescs().size();
    const NodeId scale = findNode(player.nodes(), "Scale")->id;

    for (int i = 1; i <= 20; ++i) {
        REQUIRE(player.setParameter(scale, "factor", 1.0 + 0.1 * i).ok());
        REQUIRE(player.step().ok());
    }
    CHECK(rig.device.pipelineDescs().size() == pipelines);
    CHECK_FALSE(player.status().lastError.has_value());
}

TEST_CASE("Player: speichern und wieder laden bewahrt geänderte Parameter", "[player][parameters]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    REQUIRE(player.loadText(kParamProject).ok());
    const NodeId scale = findNode(player.nodes(), "Scale")->id;
    REQUIRE(player.setParameter(scale, "factor", 3.25).ok());
    const NodeId gradient = findNode(player.nodes(), "Linear Gradient 2")->id;
    REQUIRE(player.setParameter(gradient, "colorB", ColorRgba{0.1F, 0.2F, 0.3F, 0.4F}).ok());

    const std::string path = (std::filesystem::temp_directory_path() / "whfx_player_save_test.whfx").string();
    REQUIRE(player.save(path).ok());
    CHECK(player.status().path == path);

    runtime::Player other{&rig.device, *rig.library, rig.gate};
    REQUIRE(other.load(path).ok());
    CHECK(std::get<double>(findParameter(*findNode(other.nodes(), "Scale"), "factor")->value) == 3.25);
    CHECK(std::get<ColorRgba>(findParameter(*findNode(other.nodes(), "Linear Gradient 2"), "colorB")->value) ==
          ColorRgba{0.1F, 0.2F, 0.3F, 0.4F});
    std::filesystem::remove(path);

    CHECK_FALSE(player.save("/nicht/vorhanden/x.whfx").ok());
}

TEST_CASE("Player: speichern ohne Projekt ist ein Fehler", "[player][parameters]") {
    Rig rig;
    runtime::Player player{&rig.device, *rig.library, rig.gate};
    CHECK_FALSE(player.save("/tmp/x.whfx").ok());
}
