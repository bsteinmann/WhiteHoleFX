// Lightning und Particles auf einem echten Gerät. Beide Nodes würfeln mit einem Hash; die Tests
// prüfen darum Eigenschaften, die nicht vom Zufallswert abhängen (Blitz hängt oben und endet bei
// `reach`, ein ruhendes Teilchen sitzt am Ausgangspunkt, Richtung und Schwerkraft stimmen), und
// keine einzelnen Pixel eines bestimmten Bilds. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

/// Summe des Alphakanals in den Spalten [x0, x1) und Zeilen [y0, y1).
float alphaSum(const Pixels& image, std::uint32_t x0, std::uint32_t x1, std::uint32_t y0, std::uint32_t y1) {
    float sum = 0.0F;
    for (std::uint32_t y = y0; y < y1; ++y) {
        for (std::uint32_t x = x0; x < x1; ++x) {
            sum += image.at(x, y)[3];
        }
    }
    return sum;
}

/// Ein einzelnes Teilchen, das ohne Bewegung am Ausgangspunkt (0.5, 0.8) steht.
NodeId restingParticle(Lab& lab) {
    const NodeId node = lab.add("wh.generator.particles");
    lab.set(node, "count", std::int64_t{1});
    lab.set(node, "lifetime", 2.0);
    lab.set(node, "speed", 0.0);
    lab.set(node, "gravity", 0.0);
    lab.set(node, "trail", 0.0);
    lab.set(node, "size", 0.2);
    lab.set(node, "ex", 0.5);
    lab.set(node, "ey", 0.8);
    lab.set(node, "colorA", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    lab.set(node, "colorB", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    return node;
}

}  // namespace

TEST_CASE("Lightning: hängt oben, endet bei reach, bei flash 0 bleibt der Rest dunkel", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId bolt = lab.add("wh.generator.lightning");
    lab.set(bolt, "rate", 1.0);
    lab.set(bolt, "duration", 5.0);  // sichtbar über den ganzen Takt, damit jede Zeit einen Blitz zeigt
    lab.set(bolt, "flash", 0.0);
    lab.set(bolt, "branches", 0.0);
    lab.set(bolt, "reach", 0.4);
    lab.set(bolt, "color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});

    for (const double seconds : {0.0, 0.2, 3.1, 7.6}) {
        const Pixels image = render(*device, lab, bolt, 20, 40, seconds);
        CHECK(alphaSum(image, 0, 20, 0, 1) > 0.3F);    // oberste Zeile: der Blitz hängt an der Wolke
        CHECK(alphaSum(image, 0, 20, 8, 12) > 0.3F);   // im Reach (dy = 0.2..0.3) noch zu sehen
        CHECK(alphaSum(image, 0, 20, 30, 40) == 0.0F);  // unterhalb von reach (dy > 0.75) nichts
    }
    // Ein Blitz ist schmal: in einer Zeile leuchten nur wenige Pixel
    const Pixels image = render(*device, lab, bolt, 20, 40, 0.0);
    CHECK(alphaSum(image, 0, 20, 5, 6) < 8.0F);
}

TEST_CASE("Lightning: flash hellt die ganze Fläche auf, bolts bringt mehr Licht", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId bolt = lab.add("wh.generator.lightning");
    lab.set(bolt, "rate", 1.0);
    lab.set(bolt, "duration", 5.0);
    lab.set(bolt, "branches", 0.0);
    lab.set(bolt, "reach", 0.2);
    lab.set(bolt, "flash", 0.0);
    const Pixels dark = render(*device, lab, bolt, 16, 16, 0.0);
    CHECK(closeTo(dark.at(15, 15)[3], 0.0F));  // weit weg vom Blitz und ohne Flash: durchsichtig
    lab.set(bolt, "flash", 1.0);
    const Pixels flashed = render(*device, lab, bolt, 16, 16, 0.0);
    CHECK(flashed.at(15, 15)[3] > 0.3F);  // die ganze Fläche leuchtet mit
    CHECK(flashed.at(0, 15)[3] > 0.3F);
}

TEST_CASE("Particles: ein ruhendes Teilchen sitzt am Ausgangspunkt", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = restingParticle(lab);
    const Pixels image = render(*device, lab, node, 20, 20, 0.0);
    // Ausgangspunkt (0.5, 0.8): Spalte 10, Zeile 4 (p.y = 1 - (4 + 0.5) / 20 = 0.775)
    CHECK(isColor(image.at(10, 4), 1.0F, 0.0F, 0.0F, 1.0F));
    CHECK(closeTo(image.at(0, 19)[3], 0.0F));
    CHECK(closeTo(image.at(19, 19)[3], 0.0F));
    CHECK(closeTo(image.at(10, 19)[3], 0.0F));
}

TEST_CASE("Particles: Richtung, Schwerkraft und Palette", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    {  // direction 0 (nach rechts), ohne Schwerkraft: nur rechts vom Ausgangspunkt etwas
        Lab lab;
        const NodeId node = restingParticle(lab);
        lab.set(node, "speed", 0.5);  // nach 1 s: 0.2 .. 0.5 rechts vom Ausgangspunkt
        lab.set(node, "direction", 0.0);
        lab.set(node, "spread", 0.0);
        lab.set(node, "size", 0.1);
        lab.set(node, "ey", 0.5);
        const Pixels image = render(*device, lab, node, 40, 40, 1.0);  // Alter 1 s von 2 s
        CHECK(alphaSum(image, 20, 40, 0, 40) > 0.2F);
        CHECK(alphaSum(image, 0, 19, 0, 40) == 0.0F);
    }
    {  // Schwerkraft zieht nach unten: bei Alter 1 s liegt das Teilchen bei y = 0.8 - 0.5 = 0.3
        Lab lab;
        const NodeId node = restingParticle(lab);
        lab.set(node, "gravity", 1.0);
        lab.set(node, "size", 0.1);
        const Pixels image = render(*device, lab, node, 20, 20, 1.0);
        CHECK(alphaSum(image, 0, 20, 12, 16) > 0.2F);   // y = 0.2 .. 0.4
        CHECK(alphaSum(image, 0, 20, 0, 8) == 0.0F);    // oben, wo es gestartet ist: nichts mehr
    }
    {  // Regenbogen: Farbe kommt aus dem Hash, nicht aus A und B (colorA/colorB sind dann egal)
        Lab lab;
        const NodeId node = restingParticle(lab);
        lab.set(node, "palette", std::int64_t{1});
        lab.set(node, "colorA", ColorRgba{0.0F, 0.0F, 0.0F, 0.0F});
        lab.set(node, "colorB", ColorRgba{0.0F, 0.0F, 0.0F, 0.0F});
        const Pixels image = render(*device, lab, node, 20, 20, 0.0);
        const auto c = image.at(10, 4);
        CHECK(closeTo(c[3], 1.0F));
        CHECK(std::max({c[0], c[1], c[2]}) > 0.99F);  // ein Kanal voll
        CHECK(std::min({c[0], c[1], c[2]}) < 0.01F);  // ein Kanal aus: voll gesättigt
    }
}
