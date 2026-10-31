// Kaleidoskop, Tunnel, Wirbel und Color Map auf einem echten Gerät. Als Eingang dient der Verlauf in x
// (Rot = x), an dem sich die Abtastkoordinate direkt ablesen lässt. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

float py(std::uint32_t row, std::uint32_t height) { return 1.0F - (static_cast<float>(row) + 0.5F) / static_cast<float>(height); }

}  // namespace

TEST_CASE("Kaleidoscope: die Faltung bleibt in einem Viertel des Eingangs und spiegelt das Bild", "[gpu_device][nodes][warps]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId ramp = lab.add("wh.generator.gradient");
    lab.set(ramp, "angle", 30.0);  // nicht symmetrisch
    const NodeId kal = lab.add("wh.transform.kaleidoscope");
    lab.set(kal, "segments", std::int64_t{2});
    lab.connect(ramp, "out", kal, "source");
    const Pixels image = render(*device, lab, kal, 20, 20);
    for (std::uint32_t y = 0; y < 20; ++y) {
        for (std::uint32_t x = 0; x < 10; ++x) {
            INFO("Pixel " << x << "," << y);
            for (int channel = 0; channel < 3; ++channel) {
                CHECK(closeTo(image.at(x, y)[channel], image.at(19 - x, y)[channel], 4e-3F));  // links = rechts gespiegelt
            }
        }
    }
    // Mit zwei Segmenten liegt der Abtastpunkt immer rechts von der Mitte (x >= 0.5): Rot (x) ist nie unter 0.5
    // des Verlaufs; die Ecke links oben zeigt denselben Wert wie die rechts oben.
    CHECK(closeTo(image.at(0, 0)[0], image.at(19, 0)[0], 4e-3F));
}

TEST_CASE("Vortex: ohne Drehung identisch, am Rand und in der Mitte unverändert, dazwischen gedreht", "[gpu_device][nodes][warps]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId ramp = rampX(lab);
    const NodeId vortex = lab.add("wh.transform.vortex");
    lab.connect(ramp, "out", vortex, "source");
    lab.set(vortex, "turns", 0.0);
    const Pixels same = render(*device, lab, vortex, 20, 20);
    for (std::uint32_t x = 0; x < 20; ++x) {
        CHECK(closeTo(same.at(x, 7)[0], centerX(x, 20), 3e-3F));
    }
    lab.set(vortex, "turns", 1.0);
    const Pixels swirled = render(*device, lab, vortex, 20, 20);
    CHECK(closeTo(swirled.at(0, 0)[0], centerX(0, 20), 3e-3F));    // Ecke: außerhalb des Radius
    CHECK(closeTo(swirled.at(19, 19)[0], centerX(19, 20), 3e-3F));
    // Pixel (14, 9): dx = 0.225, dy = 0.025; Radius 0.5; die Drehung folgt -2π · turns · (1 - r/radius)²
    const double dx = centerX(14, 20) - 0.5;
    const double dy = py(9, 20) - 0.5;
    const double k = 1.0 - std::sqrt(dx * dx + dy * dy) / 0.5;
    const double a = -2.0 * std::numbers::pi * k * k;
    const double expected = 0.5 + std::cos(a) * dx - std::sin(a) * dy;
    CHECK(closeTo(swirled.at(14, 9)[0], static_cast<float>(expected), 4e-3F));
    CHECK(std::abs(swirled.at(14, 9)[0] - centerX(14, 20)) > 0.1F);  // wirklich verändert
}

TEST_CASE("Tunnel: der Winkel um die Mitte wird zur x-Koordinate des Eingangs, die Mitte blendet aus", "[gpu_device][nodes][warps]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId ramp = rampX(lab);
    const NodeId tunnel = lab.add("wh.transform.tunnel");
    lab.set(tunnel, "repeat", std::int64_t{1});
    lab.set(tunnel, "speed", 0.0);
    lab.connect(ramp, "out", tunnel, "source");
    const Pixels image = render(*device, lab, tunnel, 21, 21);
    CHECK(closeTo(image.at(10, 5)[0], 0.25F, 4e-3F));   // oberhalb der Mitte: Winkel 90° = Viertelumlauf
    CHECK(closeTo(image.at(5, 10)[0], 0.5F, 4e-3F));    // links: 180°
    CHECK(closeTo(image.at(10, 15)[0], 0.75F, 4e-3F));  // unterhalb: 270°
    CHECK(closeTo(image.at(10, 5)[3], 1.0F));           // weit genug von der Mitte: deckend
    CHECK(image.at(10, 10)[3] < 0.05F);                 // in der Mitte ausgeblendet
    lab.set(tunnel, "fade", 0.0);
    CHECK(closeTo(render(*device, lab, tunnel, 21, 21).at(10, 10)[3], 1.0F));  // ohne Fade deckend
}

TEST_CASE("Color Map: Helligkeit wird zur Palette, Ring und Verschiebung, Alpha bleibt", "[gpu_device][nodes][warps]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId gray = solid(lab, 0.5F, 0.5F, 0.5F);  // Luma 0.5
    const NodeId map = lab.add("wh.modifier.colormap");
    lab.connect(gray, "out", map, "source");
    lab.set(map, "count", std::int64_t{3});
    lab.set(map, "color1", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    lab.set(map, "color2", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    lab.set(map, "color3", ColorRgba{0.0F, 0.0F, 1.0F, 1.0F});
    CHECK(isColor(render(*device, lab, map, 2, 2).at(0, 0), 1.0F, 0.0F, 0.0F));  // Mitte von 3 Farben = Farbe 2
    lab.set(map, "shift", 0.5);                                                    // Luma 1.0: letzte Farbe
    CHECK(isColor(render(*device, lab, map, 2, 2).at(0, 0), 0.0F, 0.0F, 1.0F));
    lab.set(map, "shift", -0.25);                                                  // Luma 0.25: halb zwischen 1 und 2
    CHECK(isColor(render(*device, lab, map, 2, 2).at(0, 0), 0.5F, 0.0F, 0.0F));
    lab.set(map, "count", std::int64_t{2});                                        // nur Farbe 1 (schwarz) und 2 (rot)
    lab.set(map, "shift", 0.0);
    lab.set(map, "wrap", true);                                                    // Ring mit 2 Farben: Luma 0.5 -> x = 1 = Farbe 2
    CHECK(isColor(render(*device, lab, map, 2, 2).at(0, 0), 1.0F, 0.0F, 0.0F));
    lab.set(map, "shift", 0.25);                                                   // Luma 0.75 im Ring: halb zurück zu Farbe 1
    CHECK(isColor(render(*device, lab, map, 2, 2).at(0, 0), 0.5F, 0.0F, 0.0F));
}

TEST_CASE("Color Map: durchsichtige Eingänge bleiben durchsichtig", "[gpu_device][nodes][warps]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId stars = lab.add("wh.generator.starfield");
    lab.set(stars, "density", 0.0);
    const NodeId map = lab.add("wh.modifier.colormap");
    lab.connect(stars, "out", map, "source");
    CHECK(closeTo(render(*device, lab, map, 4, 4).at(1, 1)[3], 0.0F));
}
