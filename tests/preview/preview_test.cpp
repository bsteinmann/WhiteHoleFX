// Tests der Vorschau: Lage, Farben, Tiefe und Fehlerfälle.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <algorithm>
#include <functional>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/preview/preview.hpp"

using namespace whitehole;
using namespace whitehole::preview;

namespace {

mapping::LedMap bake(mapping::FixtureShape shape, Vec3 origin = {}) {
    mapping::Layout layout;
    REQUIRE(layout.add("F", origin, std::move(shape)).ok());
    return layout.bake().value();
}

struct Mean {
    double x{0};
    double y{0};
    std::size_t n{0};
};

Mean meanOf(const Image& image, const std::function<bool(std::uint8_t, std::uint8_t, std::uint8_t)>& test) {
    Mean m;
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const std::uint8_t* px = &image.rgba[(std::size_t{y} * image.width + x) * 4];
            if (test(px[0], px[1], px[2])) {
                m.x += x;
                m.y += y;
                ++m.n;
            }
        }
    }
    if (m.n > 0) {
        m.x /= static_cast<double>(m.n);
        m.y /= static_cast<double>(m.n);
    }
    return m;
}

bool isRed(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return r > 150 && g < 80 && b < 80; }
bool isGreen(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return g > 150 && r < 80 && b < 80; }
bool isWhite(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return r > 200 && g > 200 && b > 200; }

constexpr ColorRgba kRed{1, 0, 0, 1};
constexpr ColorRgba kGreen{0, 1, 0, 1};
constexpr ColorRgba kWhite{1, 1, 1, 1};
constexpr ColorRgba kBlack{0, 0, 0, 1};

}  // namespace

TEST_CASE("sRGB: Umrechnung und Begrenzung", "[preview]") {
    CHECK(toSrgb8(0.0F) == 0);
    CHECK(toSrgb8(1.0F) == 255);
    CHECK(toSrgb8(0.5F) == 188);
    CHECK(toSrgb8(0.0031308F) == 10);
    CHECK(toSrgb8(-3.0F) == 0);
    CHECK(toSrgb8(7.0F) == 255);
    CHECK(toSrgb8(std::nanf("")) == 0);
}

TEST_CASE("Vorschau: links und rechts, oben und unten stimmen", "[preview]") {
    // 4 Spalten, 2 Zeilen. Zeile 0 ist oben (Kabelreihenfolge: zeilenweise).
    const mapping::LedMap map = bake(mapping::MatrixShape{4, 2, {1, 1}, {}});
    const std::vector<ColorRgba> colors = {kRed,   kRed,   kGreen, kGreen,   // oben: links rot, rechts grün
                                            kBlack, kBlack, kBlack, kBlack};  // unten aus
    const auto image = renderPreview(map, colors);
    REQUIRE(image.ok());
    CHECK(image.value().width == 960);
    CHECK(image.value().height == 540);

    const Mean red = meanOf(image.value(), isRed);
    const Mean green = meanOf(image.value(), isGreen);
    REQUIRE(red.n > 100);
    REQUIRE(green.n > 100);
    CHECK(red.x < 480);
    CHECK(green.x > 480);
    CHECK(red.y < 270);   // obere Zeile
    CHECK(green.y < 270);
    CHECK(std::abs(red.y - green.y) < 2.0);
    // Die Mitten der beiden Hälften liegen symmetrisch zur Bildmitte.
    CHECK(std::abs((red.x + green.x) / 2.0 - 480.0) < 2.0);
}

TEST_CASE("Vorschau: ausgeschaltete LEDs bleiben sichtbar", "[preview]") {
    const mapping::LedMap map = bake(mapping::MatrixShape{8, 8, {1, 1}, {}});
    const std::vector<ColorRgba> colors(64, kBlack);
    const auto image = renderPreview(map, colors);
    REQUIRE(image.ok());
    std::size_t differing = 0;
    for (std::size_t i = 0; i < image.value().rgba.size(); i += 4) {
        if (image.value().rgba[i] != 14) {
            ++differing;
        }
    }
    CHECK(differing > 64 * 20);  // 64 Punkte mit mehreren Pixeln
}

TEST_CASE("Vorschau: ein Strip füllt die Breite, nicht die Höhe", "[preview]") {
    const mapping::LedMap map = bake(mapping::StripShape{50, {0, 0, 0}, {50, 0, 0}});
    const std::vector<ColorRgba> colors(50, kWhite);
    const auto image = renderPreview(map, colors);
    REQUIRE(image.ok());
    std::uint32_t minX = 960, maxX = 0, minY = 540, maxY = 0;
    for (std::uint32_t y = 0; y < 540; ++y) {
        for (std::uint32_t x = 0; x < 960; ++x) {
            const std::uint8_t* px = &image.value().rgba[(std::size_t{y} * 960 + x) * 4];
            if (isWhite(px[0], px[1], px[2])) {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    CHECK(minX >= 24);
    CHECK(maxX <= 960 - 24);
    CHECK(maxX - minX > 800);
    CHECK(maxY - minY < 60);
    CHECK(std::abs(static_cast<int>((minY + maxY) / 2) - 270) <= 1);
}

TEST_CASE("Vorschau: Alpha dunkelt die LED ab", "[preview]") {
    const mapping::LedMap map = bake(mapping::StripShape{1, {0, 0, 0}, {1, 0, 0}});
    const auto full = renderPreview(map, std::vector<ColorRgba>{ColorRgba{1, 1, 1, 1}});
    const auto half = renderPreview(map, std::vector<ColorRgba>{ColorRgba{1, 1, 1, 0.25F}});
    REQUIRE(full.ok());
    REQUIRE(half.ok());
    const std::size_t center = (270 * 960 + 480) * 4;
    CHECK(full.value().rgba[center] == 255);
    CHECK(half.value().rgba[center] < 200);
    CHECK(half.value().rgba[center] > 40);
}

TEST_CASE("Vorschau: Tiefe wird schräg projiziert und von hinten nach vorn gezeichnet", "[preview]") {
    const mapping::LedMap cube = bake(mapping::GridShape{3, 3, 3, {1, 1, 1}, {}});
    const std::vector<ColorRgba> colors(27, kWhite);
    const auto image = renderPreview(cube, colors);
    REQUIRE(image.ok());
    CHECK(meanOf(image.value(), isWhite).n > 500);

    // Reihenfolge der Punkte darf das Bild nicht ändern: nur die Tiefe zählt.
    mapping::PointsShape frontFirst;
    frontFirst.points = {{0, 0, 1}, {0.1F, 0.1F, 0}};
    mapping::PointsShape backFirst;
    backFirst.points = {{0.1F, 0.1F, 0}, {0, 0, 1}};
    const auto a = renderPreview(bake(frontFirst), std::vector<ColorRgba>{kRed, kGreen});
    const auto b = renderPreview(bake(backFirst), std::vector<ColorRgba>{kGreen, kRed});
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value().rgba == b.value().rgba);
}

TEST_CASE("Vorschau: Fehlerfälle", "[preview]") {
    const mapping::LedMap map = bake(mapping::MatrixShape{2, 2, {1, 1}, {}});
    const std::vector<ColorRgba> four(4, kWhite);
    CHECK_FALSE(renderPreview(mapping::LedMap{}, {}).ok());
    CHECK_FALSE(renderPreview(map, std::vector<ColorRgba>(3, kWhite)).ok());
    PreviewOptions o;
    o.width = 0;
    CHECK_FALSE(renderPreview(map, four, o).ok());
    o = {};
    o.width = 20000;
    CHECK_FALSE(renderPreview(map, four, o).ok());
    o = {};
    o.margin = 300;  // kein Platz mehr
    CHECK_FALSE(renderPreview(map, four, o).ok());
    o = {};
    o.width = 64;
    o.height = 64;
    CHECK(renderPreview(map, four, o).ok());
}

TEST_CASE("Layout-Farben: ein Farbton je Fixture, heller entlang des Kabels", "[preview]") {
    mapping::Layout layout;
    REQUIRE(layout.add("A", {}, mapping::StripShape{10, {0, 0, 0}, {10, 0, 0}}).ok());
    REQUIRE(layout.add("B", {0, 2, 0}, mapping::StripShape{10, {0, 0, 0}, {10, 0, 0}}).ok());
    const mapping::LedMap map = layout.bake().value();
    const std::vector<ColorRgba> colors = layoutColors(map);
    REQUIRE(colors.size() == 20);
    // Die erste LED ist dunkler als die letzte, in jedem Fixture.
    CHECK(colors[0].r + colors[0].g + colors[0].b < colors[9].r + colors[9].g + colors[9].b);
    CHECK(colors[10].r + colors[10].g + colors[10].b < colors[19].r + colors[19].g + colors[19].b);
    // Die Fixtures haben verschiedene Farbtöne (Verhältnis der Kanäle der hellsten LED).
    CHECK(colors[9] != colors[19]);
    for (const ColorRgba& c : colors) {
        CHECK(c.a == 1.0F);
        CHECK(c.r >= 0.0F);
        CHECK(c.r <= 1.0F);
        CHECK(std::max({c.r, c.g, c.b}) > 0.2F);
    }
}

TEST_CASE("Vorschau: ein dichter Strip bekommt kleinere Punkte als eine weite Matrix", "[preview]") {
    mapping::Layout layout;
    REQUIRE(layout.add("Matrix", {0, 0, 0}, mapping::MatrixShape{4, 4, {1, 1}, {}}).ok());
    REQUIRE(layout.add("Strip", {0, 6, 0}, mapping::StripShape{60, {0, 0, 0}, {3, 0, 0}}).ok());
    const mapping::LedMap map = layout.bake().value();
    std::vector<ColorRgba> colors(map.size(), kRed);
    std::fill(colors.begin() + 16, colors.end(), kGreen);
    const auto image = renderPreview(map, colors);
    REQUIRE(image.ok());

    std::uint32_t greenMinY = 540, greenMaxY = 0, redMinY = 540, redMaxY = 0;
    for (std::uint32_t y = 0; y < 540; ++y) {
        for (std::uint32_t x = 0; x < 960; ++x) {
            const std::uint8_t* px = &image.value().rgba[(std::size_t{y} * 960 + x) * 4];
            if (isGreen(px[0], px[1], px[2])) {
                greenMinY = std::min(greenMinY, y);
                greenMaxY = std::max(greenMaxY, y);
            } else if (isRed(px[0], px[1], px[2])) {
                redMinY = std::min(redMinY, y);
                redMaxY = std::max(redMaxY, y);
            }
        }
    }
    REQUIRE(greenMaxY >= greenMinY);
    CHECK(greenMaxY - greenMinY < 14);   // Strip: kleine Punkte
    CHECK(redMaxY - redMinY > 150);      // Matrix: vier Reihen großer Punkte
    CHECK(greenMaxY < redMinY);          // der Strip liegt oben (y = 6 über der Matrix)
}
