// Die Farb-Nodes: Color mit Farbton/Sättigung/Helligkeit und Color Ramp.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <variant>

#include "builtin_lab.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

namespace {

ColorRgba colorOf(Lab& lab, NodeId node, double seconds = 0.0) {
    lab.run(seconds);
    const auto stored = lab.store.value(node, "out");
    REQUIRE(stored.has_value());
    return std::get<ColorRgba>(*stored);
}

/// Wie colorOf, aber mit Zeitschritt: Nodes mit Tempo (Color Ramp) summieren die Schritte auf.
ColorRgba colorStep(Lab& lab, NodeId node, double seconds, double delta) {
    lab.run(seconds, delta);
    const auto stored = lab.store.value(node, "out");
    REQUIRE(stored.has_value());
    return std::get<ColorRgba>(*stored);
}

/// Hängt den Farbausgang an einen Verbraucher, damit der Node berechnet wird.
void consume(Lab& lab, NodeId color) {
    const NodeId gradient = lab.add("wh.generator.gradient");
    lab.connect(color, "out", gradient, "colorA");
}

}  // namespace

TEST_CASE("Color: ohne Änderung bleibt die Farbe bitgenau") {
    Lab lab;
    const NodeId color = lab.add("wh.value.color");
    consume(lab, color);
    lab.set(color, "color", ColorRgba{0.1F, 0.7F, 0.3F, 0.5F});
    CHECK(colorOf(lab, color) == ColorRgba{0.1F, 0.7F, 0.3F, 0.5F});
}

TEST_CASE("Color: Farbton verschieben, entsättigen, abdunkeln") {
    Lab lab;
    const NodeId color = lab.add("wh.value.color");
    consume(lab, color);
    lab.set(color, "color", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});

    lab.set(color, "hue", 1.0 / 3.0);  // ein Drittel Umlauf: Rot -> Grün
    ColorRgba c = colorOf(lab, color);
    CHECK(c.r == Approx(0.0F).margin(1e-5));
    CHECK(c.g == Approx(1.0F).margin(1e-5));
    CHECK(c.b == Approx(0.0F).margin(1e-5));

    lab.set(color, "hue", 1.0);  // ein ganzer Umlauf: wieder Rot
    c = colorOf(lab, color);
    CHECK(c.r == Approx(1.0F).margin(1e-5));
    CHECK(c.g == Approx(0.0F).margin(1e-5));

    lab.set(color, "hue", 0.0);
    lab.set(color, "saturation", 0.0);
    c = colorOf(lab, color);
    CHECK(c.r == Approx(1.0F).margin(1e-5));
    CHECK(c.g == Approx(1.0F).margin(1e-5));  // weiß

    lab.set(color, "saturation", 1.0);
    lab.set(color, "brightness", 0.25);
    c = colorOf(lab, color);
    CHECK(c.r == Approx(0.25F).margin(1e-5));
    CHECK(c.g == Approx(0.0F).margin(1e-5));
}

TEST_CASE("Color: ein LFO am Farbton-Eingang färbt durch den Regenbogen") {
    Lab lab;
    const NodeId color = lab.add("wh.value.color");
    consume(lab, color);
    lab.set(color, "color", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    const NodeId time = lab.add("wh.value.time");
    lab.set(time, "speed", 0.25);  // ein Umlauf in 4 s
    lab.connect(time, "seconds", color, "hue");

    ColorRgba c = colorOf(lab, color, 0.0);
    CHECK(c.r == Approx(1.0F).margin(1e-4));
    c = colorOf(lab, color, 4.0 / 3.0);  // ein Drittel: grün
    CHECK(c.g == Approx(1.0F).margin(1e-4));
    CHECK(c.r == Approx(0.0F).margin(1e-4));
}

TEST_CASE("Color Ramp: zwischen zwei Farben überblenden") {
    Lab lab;
    const NodeId ramp = lab.add("wh.value.colorramp");
    consume(lab, ramp);
    lab.set(ramp, "color1", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    lab.set(ramp, "color2", ColorRgba{1.0F, 0.5F, 0.0F, 1.0F});
    lab.set(ramp, "loop", false);
    lab.set(ramp, "speed", 0.0);

    lab.set(ramp, "position", 0.0);
    CHECK(colorOf(lab, ramp) == ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    lab.set(ramp, "position", 0.5);
    ColorRgba c = colorOf(lab, ramp);
    CHECK(c.r == Approx(0.5F));
    CHECK(c.g == Approx(0.25F));
    lab.set(ramp, "position", 1.0);
    CHECK(colorOf(lab, ramp) == ColorRgba{1.0F, 0.5F, 0.0F, 1.0F});
    lab.set(ramp, "position", 7.0);  // ohne Loop bleibt es an der letzten Farbe
    CHECK(colorOf(lab, ramp) == ColorRgba{1.0F, 0.5F, 0.0F, 1.0F});
}

TEST_CASE("Color Ramp: drei Farben im Kreis") {
    Lab lab;
    const NodeId ramp = lab.add("wh.value.colorramp");
    consume(lab, ramp);
    lab.set(ramp, "count", std::int64_t{3});
    lab.set(ramp, "speed", 0.0);
    lab.set(ramp, "color1", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    lab.set(ramp, "color2", ColorRgba{0.0F, 1.0F, 0.0F, 1.0F});
    lab.set(ramp, "color3", ColorRgba{0.0F, 0.0F, 1.0F, 1.0F});

    lab.set(ramp, "position", 1.0 / 3.0);
    CHECK(colorOf(lab, ramp).g == Approx(1.0F).margin(1e-5));
    lab.set(ramp, "position", 2.0 / 3.0);
    CHECK(colorOf(lab, ramp).b == Approx(1.0F).margin(1e-5));
    lab.set(ramp, "position", 5.0 / 6.0);  // halb zwischen Blau und Rot (zurück zum Anfang)
    ColorRgba c = colorOf(lab, ramp);
    CHECK(c.r == Approx(0.5F).margin(1e-5));
    CHECK(c.b == Approx(0.5F).margin(1e-5));
    lab.set(ramp, "position", 1.0);  // ein voller Umlauf = wieder die erste Farbe
    CHECK(colorOf(lab, ramp).r == Approx(1.0F).margin(1e-5));
    lab.set(ramp, "position", 2.0 + 1.0 / 3.0);
    CHECK(colorOf(lab, ramp).g == Approx(1.0F).margin(1e-4));
}

TEST_CASE("Color Ramp: ein LFO am Positionseingang wechselt die Farben") {
    Lab lab;
    const NodeId ramp = lab.add("wh.value.colorramp");
    consume(lab, ramp);
    lab.set(ramp, "color1", ColorRgba{0.0F, 0.0F, 1.0F, 1.0F});
    lab.set(ramp, "color2", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    lab.set(ramp, "loop", false);
    lab.set(ramp, "speed", 0.0);
    const NodeId lfo = lab.add("wh.value.lfo");
    lab.set(lfo, "frequency", 0.25);  // Sinus über 4 s
    lab.connect(lfo, "out", ramp, "position");

    const ColorRgba start = colorOf(lab, ramp, 0.0);   // Sinus startet bei 0,5: halb gemischt
    CHECK(start.r == Approx(0.5F).margin(1e-3));
    const ColorRgba peak = colorOf(lab, ramp, 1.0);    // Gipfel: ganz Rot
    CHECK(peak.r == Approx(1.0F).margin(1e-3));
    const ColorRgba trough = colorOf(lab, ramp, 3.0);  // Tal: ganz Blau
    CHECK(trough.b == Approx(1.0F).margin(1e-3));
}

TEST_CASE("Color Ramp: mit Tempo wechselt er die Farben von selbst") {
    Lab lab;
    const NodeId ramp = lab.add("wh.value.colorramp");
    consume(lab, ramp);
    lab.set(ramp, "count", std::int64_t{3});
    lab.set(ramp, "color1", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    lab.set(ramp, "color2", ColorRgba{0.0F, 1.0F, 0.0F, 1.0F});
    lab.set(ramp, "color3", ColorRgba{0.0F, 0.0F, 1.0F, 1.0F});
    lab.set(ramp, "speed", 0.25);  // ein Umlauf in 4 s

    // Die Phase summiert die Zeitschritte auf: Nach einer Sekunde (acht Schritte à 0,125 s) steht sie bei 0,25.
    ColorRgba c{};
    for (int i = 0; i < 8; ++i) {
        c = colorStep(lab, ramp, 0.125 * (i + 1), 0.125);
    }
    const ColorRgba quarter = c;  // Phase 0,25 = 0,75 Segmente: Rot -> Grün zu 75 %
    CHECK(quarter.r == Approx(0.25F).margin(1e-3));
    CHECK(quarter.g == Approx(0.75F).margin(1e-3));
    for (int i = 8; i < 32; ++i) {
        c = colorStep(lab, ramp, 0.125 * (i + 1), 0.125);  // insgesamt 4 s = voller Umlauf
    }
    CHECK(c.r == Approx(1.0F).margin(1e-3));      // wieder die erste Farbe
    CHECK(c.g == Approx(0.0F).margin(1e-3));
}

TEST_CASE("Color Ramp: Tempo 0 hält die Farbe, negatives Tempo läuft rückwärts, ohne Loop pendelt er", "[colors]") {
    Lab lab;
    const NodeId ramp = lab.add("wh.value.colorramp");
    consume(lab, ramp);
    lab.set(ramp, "color1", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    lab.set(ramp, "color2", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.set(ramp, "loop", false);
    lab.set(ramp, "speed", 0.5);  // ein Umlauf in 2 s: 1 s hin, 1 s zurück
    ColorRgba c{};
    for (int i = 0; i < 8; ++i) {
        c = colorStep(lab, ramp, 0.125 * (i + 1), 0.125);
    }
    CHECK(c.r == Approx(0.5F).margin(1e-3));  // nach 1 s: Phase 0,5 = halbe Strecke
    for (int i = 8; i < 16; ++i) {
        c = colorStep(lab, ramp, 0.125 * (i + 1), 0.125);
    }
    CHECK(c.r == Approx(0.0F).margin(1e-3));  // nach 2 s wieder am Anfang (kein Sprung)
}
