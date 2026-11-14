// Expanding Shapes auf einem echten Gerät: neue Formen (0.86.0), Füllungen und der One-shot-Lauf. Geprüft werden
// Punkte, deren Zugehörigkeit zur Form sich von Hand ausrechnen lässt (20x20, Mitte 0.5/0.5, Pixel (x, y) hat die
// Koordinate ((x + 0.5) / 20, 1 - (y + 0.5) / 20)). Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

/// Eine einzelne, gefüllte Form (One shot bei progress 0.5, reach 0.8: Umkreisradius 0.4) ohne Verblassen.
NodeId solidShape(Lab& lab, std::int64_t shape) {
    const NodeId node = lab.add("wh.generator.shapes");
    lab.set(node, "shape", shape);
    lab.set(node, "fill", std::int64_t{1});
    lab.set(node, "run", std::int64_t{1});
    lab.set(node, "progress", 0.5);
    lab.set(node, "reach", 0.8);
    lab.set(node, "softness", 0.0);
    lab.set(node, "fade", 0.0);
    lab.set(node, "space", std::int64_t{0});
    return node;
}

}  // namespace

TEST_CASE("Expanding Shapes: One shot zeigt eine gefüllte Form, bei progress 0 und 1 nichts", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId circle = solidShape(lab, 0);
    const Pixels image = render(*device, lab, circle, 20, 20);
    CHECK(closeTo(image.at(10, 10)[3], 1.0F));  // Mitte: innen
    CHECK(closeTo(image.at(15, 10)[3], 1.0F));  // Abstand 0.275 < 0.4
    CHECK(closeTo(image.at(0, 10)[3], 0.0F));   // Abstand 0.475 > 0.4
    CHECK(closeTo(image.at(0, 0)[3], 0.0F));
    lab.set(circle, "progress", 0.0);
    CHECK(closeTo(render(*device, lab, circle, 20, 20).at(10, 10)[3], 0.0F));
    lab.set(circle, "progress", 1.0);
    CHECK(closeTo(render(*device, lab, circle, 20, 20).at(10, 10)[3], 0.0F));
    lab.set(circle, "progress", 0.5);
    lab.set(circle, "invert", true);
    const Pixels inverted = render(*device, lab, circle, 20, 20);
    CHECK(closeTo(inverted.at(10, 10)[3], 0.0F));  // Blende: innen frei, außen voll
    CHECK(closeTo(inverted.at(0, 0)[3], 1.0F));
    lab.set(circle, "invert", false);
    lab.set(circle, "inward", true);
    lab.set(circle, "progress", 0.9);  // von außen: Radius 0.1 * 0.8 = 0.08
    const Pixels small = render(*device, lab, circle, 20, 20);
    CHECK(closeTo(small.at(10, 10)[3], 1.0F));
    CHECK(closeTo(small.at(15, 10)[3], 0.0F));
}

TEST_CASE("Expanding Shapes: Balken, Kreuz, Dreieck und Stern liegen richtig", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId bars = solidShape(lab, 8);  // waagerecht: |y - 0.5| < 0.4
    const Pixels h = render(*device, lab, bars, 20, 20);
    CHECK(closeTo(h.at(0, 10)[3], 1.0F));  // ganz links, mittlere Zeile: innerhalb
    CHECK(closeTo(h.at(10, 0)[3], 0.0F));  // oberste Zeile: |dy| = 0.475
    lab.set(bars, "shape", std::int64_t{9});  // senkrecht
    const Pixels v = render(*device, lab, bars, 20, 20);
    CHECK(closeTo(v.at(10, 0)[3], 1.0F));
    CHECK(closeTo(v.at(0, 10)[3], 0.0F));
    lab.set(bars, "shape", std::int64_t{7});  // Kreuz: Arme ein Drittel breit
    const Pixels cross = render(*device, lab, bars, 20, 20);
    CHECK(closeTo(cross.at(3, 10)[3], 1.0F));   // im waagerechten Arm (dx = 0.325 < 0.4, dy = 0.025 * 3 < 0.4)
    CHECK(closeTo(cross.at(3, 3)[3], 0.0F));    // diagonal: in keinem Arm
    CHECK(closeTo(cross.at(10, 3)[3], 1.0F));   // im senkrechten Arm
    lab.set(bars, "shape", std::int64_t{3});  // Dreieck, Spitze oben, Umkreisradius 0.4
    const Pixels tri = render(*device, lab, bars, 20, 20);
    CHECK(closeTo(tri.at(10, 3)[3], 1.0F));   // oben an der Spitze (dy = 0.325 < 0.4)
    CHECK(closeTo(tri.at(10, 16)[3], 0.0F));  // unten: die Grundlinie liegt bei dy = -0.2
    CHECK(closeTo(tri.at(10, 12)[3], 1.0F));  // dy = -0.125: noch über der Grundlinie
    lab.set(bars, "shape", std::int64_t{6});  // Stern mit 5 Zacken, eine Spitze oben
    lab.set(bars, "points", std::int64_t{5});
    const Pixels star = render(*device, lab, bars, 20, 20);
    CHECK(closeTo(star.at(10, 3)[3], 1.0F));   // Spitze oben
    CHECK(closeTo(star.at(10, 16)[3], 0.0F));  // unten zwischen zwei Zacken (innerer Radius 0.18)
    lab.set(bars, "rotation", 180.0);
    const Pixels flipped = render(*device, lab, bars, 20, 20);
    CHECK(closeTo(flipped.at(10, 16)[3], 1.0F));  // gedreht zeigt eine Spitze nach unten
}

TEST_CASE("Expanding Shapes: Squircle morpht zwischen Kreis (n=2) und Quadrat (n gross)", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    // Diagonalpunkt (0.325, 0.325) von der Mitte: Kreisabstand 0.46 > r=0.4 (aussen), Quadrat-Lp-Abstand bei
    // squareness=1 (n=12) rund 0.34 < 0.4 (innen) - der Eckpunkt unterscheidet die beiden Extreme eindeutig.
    const NodeId squircle = solidShape(lab, 11);
    lab.set(squircle, "squareness", 0.0);
    const Pixels circleLike = render(*device, lab, squircle, 20, 20);
    CHECK(closeTo(circleLike.at(16, 3)[3], 0.0F));  // wie ein Kreis: Diagonale liegt ausserhalb
    lab.set(squircle, "squareness", 1.0);
    const Pixels squareLike = render(*device, lab, squircle, 20, 20);
    CHECK(closeTo(squareLike.at(16, 3)[3], 1.0F));  // fast quadratisch: Diagonale liegt innerhalb
    CHECK(closeTo(squareLike.at(10, 10)[3], 1.0F));  // Mitte bleibt in jedem Fall innen
}

TEST_CASE("Expanding Shapes: Spiral ist am Winkel 0 eine normale Ringkette, am Winkel pi versetzt", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId spiral = lab.add("wh.generator.shapes");
    lab.set(spiral, "shape", std::int64_t{10});
    lab.set(spiral, "thickness", 1.0);
    lab.set(spiral, "fade", 0.0);
    lab.set(spiral, "speed", 0.0);
    lab.set(spiral, "space", std::int64_t{0});
    lab.set(spiral, "cx", 0.525);  // genau auf der Mitte von Pixel (10, 10), wie beim Ring-Test unten
    lab.set(spiral, "cy", 0.475);
    // Die exakte Mitte wird nicht geprüft: atan2(0, 0) ist mathematisch unbestimmt (jede winkelbasierte Form hat
    // dort einen Pol), das GPU-Ergebnis für diesen einen Pixel ist also nicht portabel vorhersagbar.
    const Pixels image = render(*device, lab, spiral, 20, 20);
    CHECK(image.at(13, 10)[3] > 0.95F);                 // Winkel 0 (positive x-Achse): d = 0.15, Ringmitte wie gewohnt
    CHECK(closeTo(image.at(7, 10)[3], 0.0F, 0.02F));    // Winkel pi, gleicher Abstand 0.15: durch den Versatz jetzt dunkel
}

TEST_CASE("Expanding Shapes: Hollow core zeigt color aussen und colorB im Kern", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = solidShape(lab, 0);
    lab.set(node, "fill", std::int64_t{4});
    lab.set(node, "core", 0.5);
    lab.set(node, "cx", 0.525);
    lab.set(node, "cy", 0.475);
    lab.set(node, "color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.set(node, "colorB", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    const Pixels image = render(*device, lab, node, 20, 20);
    CHECK(isColor(image.at(10, 10), 1.0F, 0.0F, 0.0F));  // Mitte: tief im Kern -> colorB
    CHECK(isColor(image.at(16, 10), 1.0F, 1.0F, 1.0F));  // nahe am Rand (d = 0.3 von r = 0.4) -> color
    lab.set(node, "core", 0.0);  // core = 0: wie Solid, ueberall color
    const Pixels full = render(*device, lab, node, 20, 20);
    CHECK(isColor(full.at(10, 10), 1.0F, 1.0F, 1.0F));
}

TEST_CASE("Expanding Shapes: fortlaufend bleibt wie bisher, Füllungen Ring und Verlauf", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId rings = lab.add("wh.generator.shapes");
    lab.set(rings, "thickness", 1.0);
    lab.set(rings, "fade", 0.0);
    lab.set(rings, "speed", 0.0);
    lab.set(rings, "space", std::int64_t{0});
    lab.set(rings, "cx", 0.525);  // genau auf der Mitte von Pixel (10, 10)
    lab.set(rings, "cy", 0.475);
    const Pixels ring = render(*device, lab, rings, 20, 20);
    CHECK(closeTo(ring.at(10, 10)[3], 0.0F, 0.02F));  // Mitte: d = 0, zwischen zwei Ringen (ph = 0)
    CHECK(ring.at(13, 10)[3] > 0.95F);                 // d = 0.15, ph = 0.5: Ringmitte
    lab.set(rings, "fill", std::int64_t{1});  // Solid: Streifen mit Tastgrad thickness = 1 -> alles voll
    const Pixels full = render(*device, lab, rings, 20, 20);
    CHECK(closeTo(full.at(10, 10)[3], 1.0F));
    CHECK(closeTo(full.at(0, 0)[3], 1.0F));
    lab.set(rings, "fill", std::int64_t{2});  // nach innen ausklingend: an der Kante (u = 0) hell, zur Mitte hin dunkler
    const Pixels grad = render(*device, lab, rings, 20, 20);
    // u = fract(d / 0.3) - 0.5: Pixel 14 (d = 0.2, u = 0.167) liegt außen -> 0; Pixel 12 (d = 0.1, u = -0.167) innen -> 0.83
    CHECK(closeTo(grad.at(14, 10)[3], 0.0F, 0.05F));
    CHECK(closeTo(grad.at(12, 10)[3], 0.833F, 0.03F));
}
