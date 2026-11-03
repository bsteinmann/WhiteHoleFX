// Die Muster-Generatoren (Metaballs, Starfield, Fading Pixels, Expanding Shapes, Sine Lines) auf einem
// echten Gerät: einzelne Pixel stimmen mit der von Hand gerechneten Erwartung überein. Ohne Adapter
// übersprungen. Die Zahlen sind im Browser-WebGPU-Harness (Chromium) gegengeprüft.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

TEST_CASE("Metaballs, Starfield, Fading Pixels, Expanding Shapes und Sine Lines", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    {  // Ein Ball (Zeit 0) sitzt bei (0.5, 0.85): Kern in der Blobfarbe, unten Hintergrund
        Lab lab;
        const NodeId balls = lab.add("wh.generator.metaballs");
        lab.set(balls, "count", std::int64_t{1});
        lab.set(balls, "colorB", ColorRgba{1.0F, 0.2F, 0.6F, 1.0F});
        const Pixels image = render(*device, lab, balls, 20, 20);
        CHECK(isColor(image.at(9, 2), 1.0F, 0.2F, 0.6F));
        CHECK(isColor(image.at(9, 19), 0.0F, 0.0F, 0.0F));
    }
    {  // Starfield: ohne Sterne durchsichtig, bei voller Dichte in jeder Zelle ein Stern
        Lab lab;
        const NodeId stars = lab.add("wh.generator.starfield");
        lab.set(stars, "cells", 4.0);
        lab.set(stars, "size", 1.0);
        lab.set(stars, "density", 0.0);
        const Pixels empty = render(*device, lab, stars, 4, 4);
        CHECK(closeTo(empty.at(1, 1)[3], 0.0F));
        lab.set(stars, "density", 1.0);
        const Pixels full = render(*device, lab, stars, 4, 4);
        for (std::uint32_t y = 0; y < 4; ++y) {
            for (std::uint32_t x = 0; x < 4; ++x) {
                CHECK(full.at(x, y)[3] > 0.29F);
            }
        }
    }
    {  // Fading Pixels: ohne Dichte nichts
        Lab lab;
        const NodeId fading = lab.add("wh.generator.fading");
        lab.set(fading, "density", 0.0);
        const Pixels image = render(*device, lab, fading, 8, 8, 2.0);
        CHECK(closeTo(image.at(3, 3)[3], 0.0F));
    }
    {  // Random Pixels: Dichte 0 = nichts, Dichte 1 mit Blink = jedes Pixel (4 x 4 Zellen = 4 x 4 Bildpunkte)
        Lab lab;
        const NodeId pixels = lab.add("wh.generator.randompixels");
        lab.set(pixels, "columns", std::int64_t{4});
        lab.set(pixels, "rows", std::int64_t{4});
        lab.set(pixels, "density", 0.0);
        const Pixels none = render(*device, lab, pixels, 4, 4, 1.3);
        lab.set(pixels, "density", 1.0);
        const Pixels all = render(*device, lab, pixels, 4, 4, 1.3);
        for (std::uint32_t y = 0; y < 4; ++y) {
            for (std::uint32_t x = 0; x < 4; ++x) {
                CHECK(closeTo(none.at(x, y)[3], 0.0F));
                CHECK(closeTo(all.at(x, y)[3], 1.0F));
            }
        }
    }
    {  // Expanding Shapes: Quadrat und Raute unterscheiden sich an einer Ecke; Mitte ist ein Ring
        Lab lab;
        const NodeId shapes = lab.add("wh.generator.shapes");
        lab.set(shapes, "thickness", 1.0);
        lab.set(shapes, "fade", 0.0);
        lab.set(shapes, "shape", std::int64_t{1});
        const Pixels square = render(*device, lab, shapes, 20, 20);
        lab.set(shapes, "shape", std::int64_t{2});
        const Pixels diamond = render(*device, lab, shapes, 20, 20);
        CHECK(closeTo(diamond.at(15, 9)[3], 0.0F, 0.02F));   // Raute: d = 0.3 liegt genau zwischen zwei Ringen
        CHECK(square.at(15, 9)[3] > 0.05F);                   // Quadrat: d = 0.275
    }
    {  // Sine Lines: auf der Linie die erste Farbe, daneben nichts; zwei gleiche Linien addieren sich
        Lab lab;
        const NodeId lines = lab.add("wh.generator.sinelines");
        lab.set(lines, "frequency", 1.0);
        lab.set(lines, "speed", 0.0);
        lab.set(lines, "spread", 0.0);
        lab.set(lines, "count", std::int64_t{1});
        const Pixels one = render(*device, lab, lines, 2, 2);
        CHECK(isColor(one.at(0, 0), 1.0F, 0.3F, 0.1F, 1.0F));
        CHECK(closeTo(one.at(0, 1)[3], 0.0F));
        lab.set(lines, "count", std::int64_t{2});
        const Pixels two = render(*device, lab, lines, 2, 2);
        CHECK(isColor(two.at(0, 0), 1.0F, 0.9F, 1.0F, 1.0F));
    }
}

TEST_CASE("Sphere und Box zeigen auf einer 2D-Leinwand einen Kreis und ein Rechteck", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    {  // Standard (Flat): Auf der Leinwand ist p.z = 0; die Mitte ist innen, die Ecke außen
        Lab lab;
        const NodeId sphere = lab.add("wh.generator.sphere");
        lab.set(sphere, "softness", 0.0);
        const Pixels image = render(*device, lab, sphere, 21, 21);
        CHECK(isColor(image.at(10, 10), 1.0F, 1.0F, 1.0F));
        CHECK(isColor(image.at(0, 0), 0.0F, 0.0F, 0.0F));
    }
    {
        Lab lab;
        const NodeId box = lab.add("wh.generator.box");
        lab.set(box, "softness", 0.0);
        const Pixels image = render(*device, lab, box, 21, 21);
        CHECK(isColor(image.at(10, 10), 1.0F, 1.0F, 1.0F));
        CHECK(isColor(image.at(0, 0), 0.0F, 0.0F, 0.0F));
    }
    {  // Volume: Bei cz = 0.5 schneidet die Ebene z = 0 den Körper nicht - das alte Verhalten bleibt wählbar
        Lab lab;
        const NodeId sphere = lab.add("wh.generator.sphere");
        lab.set(sphere, "softness", 0.0);
        lab.set(sphere, "space", 1.0);
        const Pixels image = render(*device, lab, sphere, 21, 21);
        CHECK(isColor(image.at(10, 10), 0.0F, 0.0F, 0.0F));
    }
}
