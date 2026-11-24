// Geteilte Vorschau auf einem echten Gerät: Die Session tastet neben der Ausgabe das verdeckte Deck ab, ohne die Ausgabe
// zu ändern, und liefert nur Farben, die zur aktuellen Wahl gehören. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>

#include "recording_sink.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/live_decks.hpp"
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

const char* const kProject = R"(whitehole-project 1
destination 0 127.0.0.1
fixture Wand
  matrix 4 2
  patch artnet destination 0 universe 1
end
node p wh.generator.solid
end
output p.out

look Rot fade 1
node r wh.generator.solid
  set color 1 0 0 1
end
output r.out
endlook

look Blau fade 1
node b wh.generator.solid
  set color 0 0 1 1
end
output b.out
endlook
)";

bool allColor(const std::vector<ColorRgba>& colors, float r, float b) {
    if (colors.empty()) {
        return false;
    }
    for (const ColorRgba& c : colors) {
        if (std::abs(c.r - r) > 1e-3F || std::abs(c.b - b) > 1e-3F) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("Geteilte Vorschau: das verdeckte Deck kommt getrennt von der Ausgabe zurück", "[gpu_device][session][secondary]") {
    const auto device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto library = nodes::BuiltinLibrary::create().value();
    auto parsed = project::parseProject(kProject, library->registry);
    REQUIRE(parsed.ok());
    project::Project project = std::move(parsed).value();

    runtime::LiveDecks decks{library->registry};
    REQUIRE(decks.start(project.looks[0], 0).ok());
    project::Project live = project;
    live.graph = decks.graph();
    live.target = decks.target();

    RecordingSink sink;
    engine::ManualFrameClock clock{Seconds{1.0 / 40.0}};
    runtime::SessionOptions options;
    options.keepLatestColors = true;
    options.secondaryColors = true;
    options.secondaryTargets = decks.deckOutputs();
    auto created = runtime::Session::create(live, runtime::SessionDeps{*device, library->snippets, library->kernels, sink, clock}, options);
    REQUIRE(created.ok());
    runtime::Session& session = *created.value();
    const auto frames = [&](int count) {
        for (int i = 0; i < count; ++i) {
            REQUIRE(session.renderFrame().ok());
            REQUIRE(session.flush(std::chrono::seconds{5}));
        }
    };

    frames(2);
    CHECK(allColor(session.latestColors(), 1.0F, 0.0F));
    CHECK(session.latestSecondaryColors().empty());  // nichts gewählt

    REQUIRE(decks.arm(project.looks[1], 1).ok());
    REQUIRE(session.applyGraph(decks.graph(), decks.deckOutputs()).ok());
    REQUIRE(decks.previewDeck().has_value());
    REQUIRE(session.selectSecondary(decks.previewDeck()).ok());
    frames(2);
    CHECK_FALSE(session.stageError().has_value());
    CHECK(allColor(session.latestColors(), 1.0F, 0.0F));           // die Ausgabe bleibt Rot
    CHECK(allColor(session.latestSecondaryColors(), 0.0F, 1.0F));  // die Vorschau zeigt Blau
    CHECK(sink.packets().back().bytes[18] == 255);                 // Art-Net: erste LED rot

    REQUIRE(session.selectSecondary(std::nullopt).ok());
    CHECK(session.latestSecondaryColors().empty());
    frames(1);
    CHECK(session.latestSecondaryColors().empty());  // ohne Wahl wird nichts mehr übernommen
}
