// Das ganze Projekt auf einem echten Gerät: Projekttext -> Session -> Art-Net-Pakete mit den
// Farben, die die CPU-Rechnung erwartet. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "recording_sink.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/color_encoder.hpp"
#include "whitehole/runtime/session.hpp"

using namespace whitehole;
using whitehole::testing::RecordingSink;

namespace {

std::unique_ptr<gpu::WebGpuDevice> makeDevice() {
    auto device = gpu::WebGpuDevice::create();
    if (!device.ok()) {
        gpu::WebGpuOptions software;
        software.forceFallbackAdapter = true;
        device = gpu::WebGpuDevice::create(software);
    }
    return device.ok() ? std::move(device).value() : nullptr;
}

// 8x2-Matrix, Verlauf von links (schwarz) nach rechts (weiß), Gamma wie im Projekt.
const char* const kProject = R"(whitehole-project 1
destination 0 127.0.0.1

fixture Wand
  matrix 8 2
  patch artnet destination 0 universe 4 channel 1 order RGB gamma 2.2 brightness 1
end

node g wh.generator.gradient
  set angle 0
end
output g.out
)";

}  // namespace

TEST_CASE("Session auf dem Gerät: die Pakete tragen die erwarteten Farben", "[gpu_device][session]") {
    const auto device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto library = nodes::BuiltinLibrary::create().value();
    auto parsed = project::parseProject(kProject, library->registry);
    REQUIRE(parsed.ok());

    RecordingSink sink;
    engine::ManualFrameClock clock{Seconds{1.0 / 40.0}};
    runtime::SessionOptions sessionOptions;
    sessionOptions.keepLatestColors = true;
    auto session = runtime::Session::create(
        parsed.value(), runtime::SessionDeps{*device, library->snippets, library->kernels, sink, clock},
        sessionOptions);
    REQUIRE(session.ok());

    for (int i = 0; i < 3; ++i) {
        REQUIRE(session.value()->renderFrame().ok());
        REQUIRE(session.value()->flush(std::chrono::seconds{5}));
    }
    CHECK_FALSE(session.value()->stageError().has_value());

    // Die abgegriffenen Fließkommafarben sind der Verlauf selbst (vor Gamma und Rundung).
    const std::vector<ColorRgba> latest = session.value()->latestColors();
    REQUIRE(latest.size() == 16);
    for (std::size_t led = 0; led < 16; ++led) {
        const float t = (static_cast<float>(led % 8) + 0.5F) / 8.0F;
        INFO("LED " << led);
        CHECK(std::abs(latest[led].r - t) < 1e-4F);
        CHECK(std::abs(latest[led].b - t) < 1e-4F);
    }
    CHECK(session.value()->stats().output.framesProcessed == 3);

    const auto packets = sink.packets();
    REQUIRE(packets.size() == 3);
    const auto& packet = packets.back();
    REQUIRE(packet.bytes.size() == 18 + 48);
    CHECK(packet.bytes[14] == 4);

    // Die Matrix hat 8 Spalten und 2 Zeilen; p.x einer LED ist (Spalte + 0,5) / 8, der Verlauf
    // mit Winkel 0 ergibt t = p.x. Reihenfolge am Kabel: zeilenweise, Zeile 0 zuerst.
    const output::ColorEncoder encoder = output::ColorEncoder::create(output::ColorSettings{}).value();
    for (std::size_t led = 0; led < 16; ++led) {
        const float t = (static_cast<float>(led % 8) + 0.5F) / 8.0F;
        const auto expected = encoder.encode(ColorRgba{t, t, t, 1.0F});
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const int actual = packet.bytes[18 + led * 3 + channel];
            INFO("LED " << led << " Kanal " << channel);
            CHECK(std::abs(actual - static_cast<int>(expected[channel])) <= 1);
        }
    }
    // Der Verlauf steigt: die letzte Spalte ist heller als die erste.
    CHECK(packet.bytes[18 + 7 * 3] > packet.bytes[18]);
}

TEST_CASE("Session auf dem Gerät: die Zeit bewegt das Bild", "[gpu_device][session]") {
    const auto device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto library = nodes::BuiltinLibrary::create().value();
    // Der Winkel hängt an der Zeit (90 Grad je Sekunde): nach einer Sekunde steigt der Verlauf
    // von unten nach oben statt von links nach rechts.
    auto parsed = project::parseProject(
        "whitehole-project 1\ndestination 0 127.0.0.1\n"
        "fixture Wand\n  matrix 2 2\n  patch artnet destination 0 universe 0 gamma 1\nend\n"
        "node t wh.value.time\n  set speed 90\nend\n"
        "node g wh.generator.gradient\nend\n"
        "node r wh.transform.rotate\nend\n"
        "connect g.out -> r.source\nconnect t.seconds -> r.angle\n"
        "output r.out\n",
        library->registry);
    REQUIRE(parsed.ok());

    RecordingSink sink;
    engine::ManualFrameClock clock{Seconds{1.0}};
    auto session = runtime::Session::create(
        parsed.value(), runtime::SessionDeps{*device, library->snippets, library->kernels, sink, clock});
    REQUIRE(session.ok());

    for (int i = 0; i < 3; ++i) {
        REQUIRE(session.value()->renderFrame().ok());
        REQUIRE(session.value()->flush(std::chrono::seconds{5}));
    }
    const auto packets = sink.packets();
    REQUIRE(packets.size() == 3);
    // Pakete unterscheiden sich, weil sich die Zeit (und damit der Winkel) ändert.
    CHECK(packets[0].bytes != packets[1].bytes);
    CHECK(packets[1].bytes != packets[2].bytes);
}

TEST_CASE("Session auf dem Gerät: geänderte Parameter wirken ohne Neustart", "[gpu_device][session]") {
    const auto device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto library = nodes::BuiltinLibrary::create().value();
    auto parsed = project::parseProject(kProject, library->registry);
    REQUIRE(parsed.ok());
    project::Project p = std::move(parsed).value();

    RecordingSink sink;
    engine::ManualFrameClock clock{Seconds{1.0 / 40.0}};
    runtime::SessionOptions sessionOptions;
    sessionOptions.keepLatestColors = true;
    auto session = runtime::Session::create(
        p, runtime::SessionDeps{*device, library->snippets, library->kernels, sink, clock}, sessionOptions);
    REQUIRE(session.ok());
    REQUIRE(session.value()->renderFrame().ok());
    REQUIRE(session.value()->flush(std::chrono::seconds{5}));
    const std::vector<ColorRgba> before = session.value()->latestColors();
    REQUIRE(before.size() == 16);
    CHECK(before[7].g > 0.8F);  // letzte Spalte: fast weiß

    // Farbe B auf reines Rot: Grün und Blau verschwinden, Rot bleibt.
    const NodeId node = p.graph.nodes().front().id;
    REQUIRE(p.graph.setParameter(node, "colorB", graph::ParameterValue{ColorRgba{1.0F, 0.0F, 0.0F, 1.0F}}).ok());
    REQUIRE(session.value()->applyGraph(p.graph).ok());
    REQUIRE(session.value()->renderFrame().ok());
    REQUIRE(session.value()->flush(std::chrono::seconds{5}));
    const std::vector<ColorRgba> after = session.value()->latestColors();
    REQUIRE(after.size() == 16);
    for (std::size_t led = 0; led < 16; ++led) {
        const float t = (static_cast<float>(led % 8) + 0.5F) / 8.0F;
        INFO("LED " << led);
        CHECK(std::abs(after[led].r - t) < 1e-4F);
        CHECK(after[led].g < 1e-4F);
        CHECK(after[led].b < 1e-4F);
    }
    CHECK_FALSE(session.value()->stageError().has_value());
}
