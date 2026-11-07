// Die 1D-Generatoren (Gruppe "Generators 1D") auf einem echten Gerät: Der erzeugte WGSL-Code übersetzt und läuft, und
// ein paar Pixel stimmen mit der von Hand gerechneten Erwartung überein. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

TEST_CASE("1D-Generatoren: Pegelbalken füllt bis zum Pegel, auch auf Achse y und gespiegelt", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    {
        Lab lab;
        const NodeId bar = lab.add("wh.generator1d.levelbar");
        lab.set(bar, "level", 0.5);
        const Pixels image = render(*device, lab, bar, 16, 2);
        CHECK(image.at(2, 0)[3] > 0.9F);   // links vom Pegel an
        CHECK(closeTo(image.at(13, 0)[3], 0.0F));  // rechts davon aus
    }
    {  // gespiegelt: füllt von rechts
        Lab lab;
        const NodeId bar = lab.add("wh.generator1d.levelbar");
        lab.set(bar, "level", 0.5);
        lab.set(bar, "flip", true);
        const Pixels image = render(*device, lab, bar, 16, 2);
        CHECK(closeTo(image.at(2, 0)[3], 0.0F));
        CHECK(image.at(13, 0)[3] > 0.9F);
    }
    {  // Achse y: Pegel wirkt in der Höhe, in einer Zeile ist alles gleich
        Lab lab;
        const NodeId bar = lab.add("wh.generator1d.levelbar");
        lab.set(bar, "level", 1.0);
        lab.set(bar, "axis", static_cast<std::int64_t>(1));
        const Pixels image = render(*device, lab, bar, 4, 8);
        CHECK(image.at(1, 3)[3] > 0.5F);
        CHECK(closeTo(image.at(0, 3)[3], image.at(3, 3)[3]));
    }
}

TEST_CASE("1D-Generatoren: Komet, Meteore, Funkeln, Pulswellen und Verlauf erzeugen Licht und bleiben im Bereich",
          "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    for (const char* typeId : {"wh.generator1d.comet", "wh.generator1d.meteors", "wh.generator1d.sparkle",
                               "wh.generator1d.pulses", "wh.generator1d.gradient"}) {
        INFO(typeId);
        Lab lab;
        const NodeId node = lab.add(typeId);
        const Pixels image = render(*device, lab, node, 32, 2);
        float brightest = 0.0F;
        for (std::uint32_t x = 0; x < 32; ++x) {
            for (int c = 0; c < 4; ++c) {
                const float v = image.at(x, 0)[static_cast<std::size_t>(c)];
                REQUIRE(v >= -0.001F);
                REQUIRE(v <= 1.001F);
            }
            brightest = std::max(brightest, image.at(x, 0)[3]);
        }
        CHECK(brightest > 0.0F);
    }
}
