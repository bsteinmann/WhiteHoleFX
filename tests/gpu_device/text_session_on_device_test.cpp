// Der Text-Node im ganzen Projekt auf einem echten Gerät: Projekttext -> Session -> LED-Farben über die Zeit.
//
// Warum es diese Datei gibt: text_on_device_test prüft die Schrift auf einer Leinwand mit festen Werten. Gemeldet
// wurde aber (Issue #13), dass in der Oberfläche "kein Text" erscheint, obwohl der Hintergrund funktioniert.
// Dieser Test fährt dieselbe Strecke wie die Oberfläche (Layout 32 x 16, Standardwerte des Nodes, Laufschrift
// mit der Zeit) und verlangt, dass irgendwann Schriftpunkte auf LEDs landen. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>

#include "recording_sink.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/nodes/builtin_library.hpp"
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

/// Läuft das Projekt `frames` Frames mit 40 fps und zählt, wie viele LEDs im besten Frame "an" sind.
std::size_t brightestFrame(gpu::WebGpuDevice& device, const std::string& text, int frames) {
    auto library = nodes::BuiltinLibrary::create().value();
    auto parsed = project::parseProject(text, library->registry);
    REQUIRE(parsed.ok());
    RecordingSink sink;
    engine::ManualFrameClock clock{Seconds{1.0 / 40.0}};
    runtime::SessionOptions options;
    options.keepLatestColors = true;
    auto session = runtime::Session::create(
        parsed.value(), runtime::SessionDeps{device, library->snippets, library->kernels, sink, clock}, options);
    REQUIRE(session.ok());
    std::size_t best = 0;
    for (int i = 0; i < frames; ++i) {
        REQUIRE(session.value()->renderFrame().ok());
        REQUIRE(session.value()->flush(std::chrono::seconds{5}));
        std::size_t lit = 0;
        for (const ColorRgba& c : session.value()->latestColors()) {
            if (c.a > 0.5F && c.r + c.g + c.b > 1.5F) {
                ++lit;
            }
        }
        best = std::max(best, lit);
    }
    CHECK_FALSE(session.value()->stageError().has_value());
    return best;
}

std::string projectText(const std::string& nodeSettings) {
    return "whitehole-project 1\ndestination 0 127.0.0.1\n"
           "fixture Wand\n  matrix 32 16 serpentine\n  patch artnet destination 0 universe 0 gamma 1\nend\n"
           "node s wh.generator.text\n" +
           nodeSettings + "end\noutput s.out\n";
}

}  // namespace

TEST_CASE("Text im Projekt: die Laufschrift mit den Standardwerten erreicht die LEDs", "[gpu_device][text][session]") {
    const auto device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    // Standardwerte: weißer Text, durchsichtiger Hintergrund, 12 Punkte/s; nach 120 Frames (3 s) ist der Text im Bild.
    CHECK(brightestFrame(*device, projectText(""), 120) > 10);
}

TEST_CASE("Text im Projekt: ein eigener Text, stehend und rollend", "[gpu_device][text][session]") {
    const auto device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    CHECK(brightestFrame(*device, projectText("  set text \"Hallo\"\n  set mode 1\n"), 3) > 10);
    CHECK(brightestFrame(*device, projectText("  set text \"Hallo\"\n  set speed 40\n"), 120) > 10);
}
