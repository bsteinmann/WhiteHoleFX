// Tests: Raster des Layout-Editors (Linien im Bild, Vergröbern, Einrasten).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "whitehole/preview/grid.hpp"

using namespace whitehole;
using namespace whitehole::preview;
using Catch::Approx;

namespace {

ViewFrame frame(float scale) {
    ViewFrame f;
    f.scale = scale;  // Welt (0,0) links unten bei Pixel (0, 100)
    f.offsetY = 100.0F;
    return f;
}

}  // namespace

TEST_CASE("Einrasten: auf das nächste Vielfache der Weite", "[preview][grid]") {
    CHECK(snapToGrid(1.26F, 0.5F) == Approx(1.5F));
    CHECK(snapToGrid(1.24F, 0.5F) == Approx(1.0F));
    CHECK(snapToGrid(-0.7F, 0.5F) == Approx(-0.5F));
    CHECK(snapToGrid(3.3F, 0.0F) == Approx(3.3F));
    CHECK(snapToGrid(3.3F, -1.0F) == Approx(3.3F));
}

TEST_CASE("Raster: zu dichte Weiten werden vergröbert", "[preview][grid]") {
    CHECK(effectiveGridStep(1.0F, 20.0F) == Approx(1.0F));   // 20 Pixel
    CHECK(effectiveGridStep(1.0F, 2.0F) == Approx(5.0F));    // 2 -> 4 -> 10 Pixel: 1, 2, 5
    CHECK(effectiveGridStep(0.1F, 1.0F) == Approx(10.0F));   // 0,1 / 0,2 / 0,5 / 1 / 2 / 5 / 10
    CHECK(effectiveGridStep(0.0F, 10.0F) == 0.0F);
    CHECK(effectiveGridStep(1.0F, 0.0F) == 0.0F);
}

TEST_CASE("Raster: Linien liegen auf den Vielfachen der Weite", "[preview][grid]") {
    const Grid grid = buildGrid(frame(10.0F), 100.0F, 100.0F, 1.0F);
    REQUIRE(grid.step == Approx(1.0F));
    REQUIRE(grid.vertical.size() >= 10);
    // Welt x = 0 liegt bei Pixel 0 und ist eine Hauptlinie; x = 1 bei Pixel 10
    bool foundZero = false;
    for (const GridLine& line : grid.vertical) {
        if (line.pixel == Approx(0.0F)) {
            foundZero = true;
            CHECK(line.major);
        }
        if (line.pixel == Approx(10.0F)) {
            CHECK_FALSE(line.major);
        }
    }
    CHECK(foundZero);
    // waagrecht: Welt y = 0 liegt bei Pixel 100 (unten)
    bool foundBottom = false;
    for (const GridLine& line : grid.horizontal) {
        foundBottom = foundBottom || line.pixel == Approx(100.0F);
    }
    CHECK(foundBottom);
}

TEST_CASE("Raster: ungültige Eingaben ergeben keine Linien", "[preview][grid]") {
    CHECK(buildGrid(frame(10.0F), 100.0F, 100.0F, 0.0F).vertical.empty());
    CHECK(buildGrid(frame(10.0F), 0.0F, 100.0F, 1.0F).vertical.empty());
    CHECK(buildGrid(frame(0.0F), 100.0F, 100.0F, 1.0F).horizontal.empty());
}
