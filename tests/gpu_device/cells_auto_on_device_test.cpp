// Zellenzahl automatisch (cells = 0): Labyrinth, Starfield, Fading Pixels und Funkeln (1D) nehmen ihr Raster aus der
// echten Auflösung (@columns/@rows), statt dass man "Cells" von Hand nachpflegt. Warum diese Datei: Die Automatik
// ist nur dann richtig, wenn das Bild bei cells = 0 exakt dem Bild mit von Hand eingetragener Zahl entspricht und
// sich bei einer anderen Zahl unterscheidet. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

bool sameAlpha(const Pixels& a, const Pixels& b, std::uint32_t w, std::uint32_t h) {
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            if (!closeTo(a.at(x, y)[3], b.at(x, y)[3])) {
                return false;
            }
        }
    }
    return true;
}

NodeId mazeFor(Lab& lab) {
    const NodeId node = lab.add("wh.generator.maze");
    lab.set(node, "runners", std::int64_t{4});
    lab.set(node, "speed", 4.0);
    lab.set(node, "trail", 8.0);
    lab.set(node, "space", std::int64_t{0});  // flach, unabhängig von der Karte des Labors
    return node;
}

}  // namespace

TEST_CASE("Zellenzahl automatisch: Labyrinth folgt Breite und Höhe der Leinwand", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId maze = mazeFor(lab);
    // 32 x 16: automatisch = 32 Zellen über die Breite (aspect folgt der echten Auflösung, also 16 in der Höhe)
    const Pixels autoImage = render(*device, lab, maze, 32, 16, 1.7);
    lab.set(maze, "cells", std::int64_t{32});
    const Pixels manual32 = render(*device, lab, maze, 32, 16, 1.7);
    CHECK(sameAlpha(autoImage, manual32, 32, 16));
    lab.set(maze, "cells", std::int64_t{8});
    const Pixels manual8 = render(*device, lab, maze, 32, 16, 1.7);
    CHECK_FALSE(sameAlpha(autoImage, manual8, 32, 16));
}

TEST_CASE("Zellenzahl automatisch: Starfield und Fading Pixels", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    for (const char* type : {"wh.generator.starfield", "wh.generator.fading"}) {
        INFO(type);
        Lab lab;
        const NodeId node = lab.add(type);
        lab.set(node, "density", 0.5);
        const Pixels autoImage = render(*device, lab, node, 16, 16, 1.3);  // quadratisch: 16 x 16 Zellen
        lab.set(node, "cells", 16.0);
        const Pixels manual16 = render(*device, lab, node, 16, 16, 1.3);
        CHECK(sameAlpha(autoImage, manual16, 16, 16));
        lab.set(node, "cells", 4.0);
        const Pixels manual4 = render(*device, lab, node, 16, 16, 1.3);
        CHECK_FALSE(sameAlpha(autoImage, manual4, 16, 16));
    }
}

TEST_CASE("Zellenzahl automatisch: Starfield und Fading haben auf 32 x 16 Zellen in beiden Richtungen", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    for (const char* type : {"wh.generator.starfield", "wh.generator.fading"}) {
        INFO(type);
        Lab lab;
        const NodeId node = lab.add(type);
        lab.set(node, "density", 0.5);
        const Pixels autoImage = render(*device, lab, node, 32, 16, 1.3);
        // Von Hand ist "Cells per axis" in beiden Richtungen gleich: 32 ergibt in der Höhe doppelt so viele
        // Zellen wie LEDs. Die Automatik (32 x 16) darf deshalb nicht gleich dem manuellen Wert 32 sein.
        lab.set(node, "cells", 32.0);
        const Pixels manual32 = render(*device, lab, node, 32, 16, 1.3);
        CHECK_FALSE(sameAlpha(autoImage, manual32, 32, 16));
    }
}

TEST_CASE("Zellenzahl automatisch: Funkeln (1D) nimmt Spalten bei Achse x und Zeilen bei Achse y", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId sparkle = lab.add("wh.generator1d.sparkle");
    lab.set(sparkle, "density", 0.5);
    lab.set(sparkle, "axis", std::int64_t{0});
    const Pixels autoX = render(*device, lab, sparkle, 16, 4, 0.9);
    lab.set(sparkle, "cells", 16.0);
    const Pixels manualX = render(*device, lab, sparkle, 16, 4, 0.9);
    CHECK(sameAlpha(autoX, manualX, 16, 4));

    lab.set(sparkle, "cells", 0.0);
    lab.set(sparkle, "axis", std::int64_t{1});
    const Pixels autoY = render(*device, lab, sparkle, 16, 4, 0.9);
    lab.set(sparkle, "cells", 4.0);
    const Pixels manualY = render(*device, lab, sparkle, 16, 4, 0.9);
    CHECK(sameAlpha(autoY, manualY, 16, 4));
}
