// Phase auf einem echten Gerät: Den Speed-Regler des Labyrinths zu verstellen darf das Bild im selben Augenblick nicht
// verändern (die Läufer springen nicht), und bei Speed 0 steht die Animation. Rückmeldung vom 26.11.: Beim Anfassen des
// Reglers tauchten die Läufer an neuen Positionen auf. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

constexpr std::uint32_t kWidth = 16;
constexpr std::uint32_t kHeight = 8;

/// Eine Stage, die über mehrere Bilder lebt (anders als render(): sie behält die Phase des Reglers).
struct LongLivedCanvas {
    WebGpuDevice& device;
    Lab& lab;
    std::unique_ptr<executor::CanvasRenderStage> stage;
    executor::CanvasStageConfig config;

    LongLivedCanvas(WebGpuDevice& deviceIn, Lab& labIn, NodeId target) : device{deviceIn}, lab{labIn} {
        config.extent = Extent3{kWidth, kHeight, 1};
        config.format = TextureFormat::Rgba32Float;
        config.target = graph::PortRef{target, "out"};
        auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE(created.ok());
        stage = std::move(created.value());
        REQUIRE(stage->setGraph(lab.graph).ok());
    }

    /// Übernimmt die Reglerwerte aus dem Graphen und rendert den Zeitpunkt `seconds`.
    Pixels grab(double seconds) {
        REQUIRE(stage->setGraph(lab.graph).ok());
        const std::vector<std::byte> bytes = ::whitehole::gputest::renderCanvas(device, *stage, config.extent, config.format, seconds);
        REQUIRE(bytes.size() == std::size_t{kWidth} * kHeight * 16U);
        Pixels image;
        image.width = kWidth;
        image.height = kHeight;
        image.data.resize(std::size_t{kWidth} * kHeight * 4);
        std::memcpy(image.data.data(), bytes.data(), bytes.size());
        CHECK(device.takeErrors().empty());
        return image;
    }
};

bool sameImage(const Pixels& a, const Pixels& b) {
    for (std::size_t i = 0; i < a.data.size(); ++i) {
        if (!closeTo(a.data[i], b.data[i], 1e-4F)) {
            return false;
        }
    }
    return true;
}

NodeId mazeNode(Lab& lab) {
    const NodeId node = lab.add("wh.generator.maze");
    lab.set(node, "cells", std::int64_t{16});
    lab.set(node, "runners", std::int64_t{3});
    lab.set(node, "speed", 4.0);
    lab.set(node, "trail", 8.0);
    lab.set(node, "head", 0.0);
    lab.set(node, "grid", 0.0);
    lab.set(node, "space", std::int64_t{0});
    lab.set(node, "colorA", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.set(node, "colorB", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    return node;
}

}  // namespace

TEST_CASE("Labyrinth: den Speed-Regler zu verstellen lässt die Läufer nicht springen", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = mazeNode(lab);
    LongLivedCanvas canvas{*device, lab, node};

    const Pixels before = canvas.grab(3.0);
    lab.set(node, "speed", 9.0);
    const Pixels sameMoment = canvas.grab(3.0);
    CHECK(sameMoment.data == before.data);  // derselbe Augenblick, dieselbe Phase: dasselbe Bild
    const Pixels later = canvas.grab(3.5);
    CHECK_FALSE(sameImage(sameMoment, later));  // und danach läuft es mit dem neuen Speed weiter

    // Gegenprobe zum Vorgehen von früher: Ein frisches Bild mit Speed 9 zur Zeit 3,0 sähe völlig anders aus.
    const Pixels fresh = render(*device, lab, node, kWidth, kHeight, 3.0);
    CHECK_FALSE(sameImage(fresh, before));
}

TEST_CASE("Labyrinth: Speed 0 hält die Animation an, später wieder aufdrehen läuft von dort weiter", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = mazeNode(lab);
    LongLivedCanvas canvas{*device, lab, node};

    (void)canvas.grab(2.0);
    lab.set(node, "speed", 0.0);
    const Pixels stopped = canvas.grab(2.0);
    CHECK(sameImage(canvas.grab(5.0), stopped));
    CHECK(sameImage(canvas.grab(40.0), stopped));

    lab.set(node, "speed", 4.0);  // bei t = 40 wieder an: Das Bild springt nicht, sondern bewegt sich ab hier
    const Pixels resumed = canvas.grab(40.0);
    CHECK(sameImage(resumed, stopped));
    CHECK_FALSE(sameImage(canvas.grab(41.0), stopped));
}
