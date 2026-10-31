// Der Text-Node auf einem echten Gerät: Punkte der Schrift, Ausrichtung, Rollen in beide Richtungen, Umlautpunkte,
// Hintergrund. Die Leinwand ist 12 x 8 Pixel mit height 1 und aspect 1,5: Ein Schriftpunkt ist genau ein Pixel,
// Zeile 0 ist oben. Das "H" der Schrift hat in Spalte 0 und 4 alle Zeilen 0..6 und in 1..3 nur Zeile 3. Ohne
// Adapter übersprungen. Die Erwartungen sind im WebGPU-Testaufbau (Chromium) gegengerechnet.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

NodeId textNode(Lab& lab, const std::string& text, std::int64_t mode, std::int64_t align = 0) {
    const NodeId node = lab.add("wh.generator.text");
    lab.set(node, "text", text);
    lab.set(node, "mode", mode);
    lab.set(node, "align", align);
    lab.set(node, "height", 1.0);
    lab.set(node, "aspect", 1.5);
    lab.set(node, "speed", 1.0);
    lab.set(node, "gap", 0.0);
    return node;
}

bool on(const Pixels& image, std::uint32_t x, std::uint32_t y) { return image.at(x, y)[3] > 0.5F; }

}  // namespace

TEST_CASE("Text: Punkte der Buchstaben H und I stehen an der richtigen Stelle", "[gpu_device][nodes][text]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = textNode(lab, "HI", 1);
    const Pixels image = render(*device, lab, node, 12, 8);
    CHECK(on(image, 0, 0));
    CHECK(on(image, 0, 6));
    CHECK_FALSE(on(image, 0, 7));   // Zeile 7 ist die freie Zeile der Zelle
    CHECK(on(image, 2, 3));         // Querbalken des H
    CHECK_FALSE(on(image, 2, 2));
    CHECK_FALSE(on(image, 5, 3));   // Abstand zwischen den Buchstaben
    CHECK(on(image, 8, 0));         // Mitte des I: alle Zeilen
    CHECK(on(image, 8, 6));
    CHECK(on(image, 7, 0));         // Serife oben
    CHECK_FALSE(on(image, 7, 3));
    CHECK(isColor(image.at(0, 0), 1.0F, 1.0F, 1.0F));
}

TEST_CASE("Text: Ausrichtung, Hintergrund und Umlautpunkte", "[gpu_device][nodes][text]") {
    REQUIRE_DEVICE();
    {  // Mittig: das H (5 Punkte) in 12 Punkten beginnt bei Punkt 4
        Lab lab;
        const NodeId node = textNode(lab, "H", 1, 1);
        const Pixels image = render(*device, lab, node, 12, 8);
        CHECK(on(image, 4, 0));
        CHECK_FALSE(on(image, 3, 0));
        CHECK(on(image, 8, 0));
        CHECK_FALSE(on(image, 9, 0));
        lab.set(node, "align", std::int64_t{2});  // rechts: Punkte 7..11
        const Pixels right = render(*device, lab, node, 12, 8);
        CHECK(on(right, 7, 0));
        CHECK(on(right, 11, 0));
        CHECK_FALSE(on(right, 6, 0));
    }
    {  // Hintergrund füllt alles, was nicht Schrift ist
        Lab lab;
        const NodeId node = textNode(lab, "H", 1);
        lab.set(node, "background", ColorRgba{0.0F, 0.0F, 1.0F, 1.0F});
        const Pixels image = render(*device, lab, node, 12, 8);
        CHECK(isColor(image.at(5, 3), 0.0F, 0.0F, 1.0F));
        CHECK(isColor(image.at(0, 0), 1.0F, 1.0F, 1.0F));
        lab.set(node, "text", std::string{});  // leerer Text: nur Hintergrund
        const Pixels empty = render(*device, lab, node, 12, 8);
        CHECK(isColor(empty.at(0, 0), 0.0F, 0.0F, 1.0F));
    }
    {  // ä: die Punkte liegen in Zeile 0 über den Spalten 1 und 3
        Lab lab;
        const NodeId node = textNode(lab, "\xC3\xA4", 1);
        const Pixels image = render(*device, lab, node, 12, 8);
        CHECK(on(image, 1, 0));
        CHECK(on(image, 3, 0));
        CHECK_FALSE(on(image, 2, 0));
        CHECK(on(image, 1, 2));  // Oberkante des a
        CHECK_FALSE(on(image, 0, 1));
    }
}

TEST_CASE("Text: Laufschrift läuft nach links und rechts und beginnt von vorn", "[gpu_device][nodes][text]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId node = textNode(lab, "H", 0);  // speed 1 Punkt/s, gap 0: Periode 5 + 12 Punkte
    {
        const Pixels start = render(*device, lab, node, 12, 8, 0.0);  // beginnt hinter dem rechten Rand
        for (std::uint32_t y = 0; y < 8; ++y) {
            for (std::uint32_t x = 0; x < 12; ++x) {
                REQUIRE_FALSE(on(start, x, y));
            }
        }
        const Pixels four = render(*device, lab, node, 12, 8, 4.0);  // Punkt 0 des H liegt bei x = 8
        CHECK(on(four, 8, 0));
        CHECK_FALSE(on(four, 7, 0));
        CHECK(on(four, 11, 3));
        CHECK_FALSE(on(four, 11, 2));
        const Pixels twelve = render(*device, lab, node, 12, 8, 12.0);  // links angekommen
        CHECK(on(twelve, 0, 0));
        const Pixels wrapped = render(*device, lab, node, 12, 8, 17.0);  // eine Periode später wie bei 0
        CHECK_FALSE(on(wrapped, 0, 0));
        CHECK_FALSE(on(wrapped, 11, 0));
    }
    lab.set(node, "speed", -1.0);  // nach rechts: bei t = 4 ragt das H mit seiner letzten Spalte herein
    const Pixels reverse = render(*device, lab, node, 12, 8, 4.0);
    CHECK(on(reverse, 3, 0));
    CHECK_FALSE(on(reverse, 4, 0));
    CHECK(on(reverse, 0, 3));
    CHECK_FALSE(on(reverse, 0, 0));
}
