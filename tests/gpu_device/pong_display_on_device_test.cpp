// Pong Display auf einem echten Gerät: Ball, Schläger und Punktanzeige an den Stellen, die die Eingänge
// vorgeben. Geprüft werden Orte und Deckung, keine einzelnen Farbwerte der weichen Ränder. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

/// Pong Display ohne Netz, Punkte und Schläger, damit nur der Ball übrig bleibt.
NodeId plainBall(Lab& lab) {
    const NodeId node = lab.add("wh.generator.pong");
    lab.set(node, "dots", std::int64_t{0});
    lab.set(node, "net", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "paddleLeft", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "paddleRight", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "ball", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    lab.set(node, "ballRadius", 0.1);
    lab.set(node, "soft", 0.01);
    return node;
}

}  // namespace

TEST_CASE("Pong Display: der Ball sitzt bei ballX / ballY und ist rund", "[gpu_device][nodes][pong]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = plainBall(lab);
    lab.set(node, "ballX", 0.25);
    lab.set(node, "ballY", 0.75);
    const Pixels image = render(*device, lab, node, 40, 40, 0.0);
    // (0.25, 0.75): Spalte 10, Zeile 10 (oben ist y = 1)
    CHECK(isColor(image.at(10, 10), 1.0F, 0.0F, 0.0F, 1.0F));
    CHECK(closeTo(image.at(30, 30)[3], 0.0F));
    CHECK(closeTo(image.at(30, 10)[3], 0.0F));
    CHECK(closeTo(image.at(10, 30)[3], 0.0F));
}

TEST_CASE("Pong Display: aspect macht den Ball bei breiter Leinwand rund", "[gpu_device][nodes][pong]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = plainBall(lab);
    lab.set(node, "aspect", 2.0);
    lab.set(node, "ballX", 0.5);
    lab.set(node, "ballY", 0.5);
    const Pixels image = render(*device, lab, node, 80, 40, 0.0);
    // Radius 0,1 Höhen = 4 Zeilen = 4 Spalten: vier Pixel neben der Mitte noch rot, zehn weg nicht mehr.
    CHECK(image.at(40 + 2, 20)[3] > 0.9F);
    CHECK(image.at(40, 20 + 2)[3] > 0.9F);
    CHECK(closeTo(image.at(40 + 8, 20)[3], 0.0F));
    CHECK(closeTo(image.at(40, 20 + 8)[3], 0.0F));
}

TEST_CASE("Pong Display: Schläger stehen am Rand bei left und right", "[gpu_device][nodes][pong]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = lab.add("wh.generator.pong");
    lab.set(node, "dots", std::int64_t{0});
    lab.set(node, "net", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "ball", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "left", 0.2);   // unten
    lab.set(node, "right", 0.8);  // oben
    lab.set(node, "paddleSize", 0.2);
    lab.set(node, "margin", 0.1);
    lab.set(node, "thickness", 0.05);
    lab.set(node, "soft", 0.01);
    const Pixels image = render(*device, lab, node, 40, 40, 0.0);
    // links bei x = 0,1 (Spalte 4), y = 0,2 (Zeile 32); rechts bei x = 0,9 (Spalte 36), y = 0,8 (Zeile 8)
    CHECK(image.at(4, 32)[3] > 0.9F);
    CHECK(image.at(36, 8)[3] > 0.9F);
    CHECK(closeTo(image.at(4, 8)[3], 0.0F));
    CHECK(closeTo(image.at(36, 32)[3], 0.0F));
    CHECK(closeTo(image.at(20, 20)[3], 0.0F));
}

TEST_CASE("Pong Display: Punkte leuchten bis zum Stand, die übrigen Plätze schwach", "[gpu_device][nodes][pong]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = lab.add("wh.generator.pong");
    lab.set(node, "net", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "ball", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "paddleLeft", ColorRgba{0.0F, 1.0F, 0.0F, 1.0F});
    lab.set(node, "paddleRight", ColorRgba{0.0F, 1.0F, 0.0F, 1.0F});
    lab.set(node, "left", 0.3);  // Schläger weit weg von den Punkten
    lab.set(node, "right", 0.3);
    lab.set(node, "maxScore", 5.0);
    lab.set(node, "scoreLeft", 3.0);
    lab.set(node, "scoreRight", 1.0);
    lab.set(node, "soft", 0.01);
    lab.set(node, "emptyLevel", 0.25);
    const Pixels image = render(*device, lab, node, 100, 100, 0.0);
    // Linke Punkte ab (0,06 | 0,93) nach rechts im Abstand 0,07: Spalten 6, 13, 20, 27, 34; Zeile 7.
    CHECK(image.at(6, 7)[3] > 0.9F);
    CHECK(image.at(13, 7)[3] > 0.9F);
    CHECK(image.at(20, 7)[3] > 0.9F);
    CHECK(closeTo(image.at(27, 7)[3], 0.25F, 0.03F));
    CHECK(closeTo(image.at(34, 7)[3], 0.25F, 0.03F));
    CHECK(closeTo(image.at(41, 7)[3], 0.0F));  // nur fünf Plätze
    // Rechte Punkte ab (0,94 | 0,93) nach links: Spalten 94, 87, 80, 73, 66.
    CHECK(image.at(94, 7)[3] > 0.9F);
    CHECK(closeTo(image.at(87, 7)[3], 0.25F, 0.03F));
}

TEST_CASE("Pong Display: die Punktreihe lässt sich frei platzieren und drehen", "[gpu_device][nodes][pong]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = lab.add("wh.generator.pong");
    lab.set(node, "net", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "ball", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});
    lab.set(node, "paddleLeft", ColorRgba{0.0F, 1.0F, 0.0F, 1.0F});
    lab.set(node, "paddleRight", ColorRgba{1.0F, 0.0F, 0.0F, 0.0F});
    lab.set(node, "left", 0.9);
    lab.set(node, "maxScore", 3.0);
    lab.set(node, "scoreLeft", 3.0);
    lab.set(node, "scoreRight", 0.0);
    lab.set(node, "soft", 0.01);
    lab.set(node, "leftX", 0.5);
    lab.set(node, "leftY", 0.2);
    lab.set(node, "leftAngle", 90.0);  // senkrecht nach oben
    const Pixels image = render(*device, lab, node, 100, 100, 0.0);
    // Start (0,5 | 0,2) = Spalte 50, Zeile 80; dann 0,07 höher: Zeile 73, 66.
    CHECK(image.at(50, 80)[3] > 0.9F);
    CHECK(image.at(50, 73)[3] > 0.9F);
    CHECK(image.at(50, 66)[3] > 0.9F);
    CHECK(closeTo(image.at(57, 80)[3], 0.0F));  // keine Punkte seitlich
}
