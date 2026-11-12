// Labyrinth auf einem echten Gerät. Der Node würfelt mit einem Hash; geprüft werden darum nur Eigenschaften, die
// vom Zufallswert unabhängig sind: Es gibt Licht und Durchsichtiges, die Bahnen liegen auf dem Raster (bei
// cells = Breite in Pixeln leuchten ganze Pixel, keine Zwischenwerte abseits des Kopfs), ohne Farbe bleibt alles
// dunkel, und das Raster füllt den Hintergrund. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

float alphaSum(const Pixels& image, std::uint32_t w, std::uint32_t h) {
    float sum = 0.0F;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            sum += image.at(x, y)[3];
        }
    }
    return sum;
}

int countAbove(const Pixels& image, std::uint32_t w, std::uint32_t h, float level) {
    int n = 0;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            n += image.at(x, y)[3] > level ? 1 : 0;
        }
    }
    return n;
}

NodeId mazeNode(Lab& lab) {
    const NodeId node = lab.add("wh.generator.maze");
    lab.set(node, "cells", std::int64_t{16});
    lab.set(node, "aspect", 2.0);
    lab.set(node, "runners", std::int64_t{2});
    lab.set(node, "speed", 4.0);
    lab.set(node, "trail", 8.0);
    lab.set(node, "thickness", 0.7);
    lab.set(node, "glow", 0.3);
    lab.set(node, "head", 0.0);
    lab.set(node, "grid", 0.0);
    lab.set(node, "space", std::int64_t{0});  // flach, unabhängig von der Karte des Labors
    lab.set(node, "colorA", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.set(node, "colorB", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    return node;
}

}  // namespace

TEST_CASE("Labyrinth: Bahnen auf dem Raster, Licht und Durchsichtiges, Bewegung mit der Zeit", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = mazeNode(lab);
    const Pixels a = render(*device, lab, node, 16, 8, 1.0);
    const float sum = alphaSum(a, 16, 8);
    CHECK(sum > 2.0F);                            // zwei Läufer mit Schweif: mehrere Pixel leuchten
    CHECK(countAbove(a, 16, 8, 0.01F) < 16 * 8);  // und nicht alles
    // Mit einer Zelle je Pixel liegen die Bahnen auf Pixelmitten: höchstens die Schweiflänge beider Läufer leuchtet.
    CHECK(countAbove(a, 16, 8, 0.05F) <= 2 * 9 + 8);
    const Pixels b = render(*device, lab, node, 16, 8, 1.5);
    int changed = 0;
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            changed += closeTo(a.at(x, y)[3], b.at(x, y)[3], 0.02F) ? 0 : 1;
        }
    }
    CHECK(changed > 0);  // nach einer halben Sekunde (zwei Zellen) hat sich das Bild bewegt
}

TEST_CASE("Labyrinth: ohne Farbe dunkel, Raster füllt den Hintergrund, Kopf hellt auf", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = mazeNode(lab);
    lab.set(node, "colorA", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "colorB", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    CHECK(closeTo(alphaSum(render(*device, lab, node, 16, 8, 1.0), 16, 8), 0.0F));
    lab.set(node, "colorA", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.set(node, "colorB", ColorRgba{0.0F, 0.0F, 1.0F, 1.0F});
    lab.set(node, "grid", 1.0);
    const Pixels grid = render(*device, lab, node, 16, 8, 1.0);
    CHECK(countAbove(grid, 16, 8, 0.3F) >= 16 * 8 - 2);  // jede Zelle trägt eine Strecke durch die Mitte
    lab.set(node, "grid", 0.0);
    lab.set(node, "speed", 0.0);
    const float plain = alphaSum(render(*device, lab, node, 16, 8, 1.0), 16, 8);
    lab.set(node, "head", 2.0);
    lab.set(node, "glow", 2.0);
    CHECK(alphaSum(render(*device, lab, node, 16, 8, 1.0), 16, 8) > plain + 0.5F);  // der Leuchthof bringt Licht dazu
}
