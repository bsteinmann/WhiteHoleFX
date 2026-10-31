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

TEST_CASE("sRGB: Rückumrechnung ist die Umkehrung", "[preview]") {
    CHECK(fromSrgb8(0) == 0.0F);
    CHECK(fromSrgb8(255) == 1.0F);
    for (int v = 0; v <= 255; ++v) {
        INFO("Wert " << v);
        CHECK(toSrgb8(fromSrgb8(static_cast<std::uint8_t>(v))) == v);
    }
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

namespace {

/// Grauwert (rote Komponente) des Bildpunkts in der Mitte der ersten LED einer einzelnen Zeile.
std::uint8_t centreRed(const Image& image) {
    const std::size_t x = image.width / 2;
    const std::size_t y = image.height / 2;
    return image.rgba[(y * image.width + x) * 4];
}

}  // namespace

TEST_CASE("Vorschau: ein Verlauf sieht gleichmäßig aus (Gamma der Ausgabe)", "[preview][gamma]") {
    // Eine einzelne LED mit dem Wert 0,5: bei Gamma 2,2 zeigt der Bildschirm etwa 0,5 (Byte ~128),
    // bei Gamma 1 das doppelte Licht (sRGB 188).
    const mapping::LedMap map = bake(mapping::MatrixShape{1, 1, {1, 1}, {}});
    const std::vector<ColorRgba> colors{ColorRgba{0.5F, 0.5F, 0.5F, 1.0F}};
    PreviewOptions options;
    options.gamma = 2.2F;
    const auto defaulted = renderPreview(map, colors, options);
    REQUIRE(defaulted.ok());
    // Die Vorschau hebt Schwarz auf einen dunklen Grauwert an (kOffLed = 34): byte = 34 + wert * (1 - 34/255).
    const auto shown = [](int value) { return 34 + static_cast<int>(std::lround(value * (1.0 - 34.0 / 255.0))); };
    CHECK(std::abs(static_cast<int>(centreRed(defaulted.value())) - shown(128)) <= 3);

    options.gamma = 1.0F;
    const auto linear = renderPreview(map, colors, options);
    REQUIRE(linear.ok());
    CHECK(std::abs(static_cast<int>(centreRed(linear.value())) - shown(188)) <= 3);
}

TEST_CASE("Vorschau: Gamma je Fixture", "[preview][gamma]") {
    mapping::Layout layout;
    REQUIRE(layout.add("A", Vec3{}, mapping::MatrixShape{1, 1, {1, 1}, {}}).ok());
    REQUIRE(layout.add("B", Vec3{4.0F, 0.0F, 0.0F}, mapping::MatrixShape{1, 1, {1, 1}, {}}).ok());
    const mapping::LedMap map = layout.bake().value();
    const std::vector<ColorRgba> colors(2, ColorRgba{0.5F, 0.5F, 0.5F, 1.0F});
    PreviewOptions options;
    options.fixtureGamma = {2.2F, 1.0F};
    const auto image = renderPreview(map, colors, options);
    REQUIRE(image.ok());
    const Image& img = image.value();
    // Die linke Hälfte (A) ist dunkler als die rechte (B).
    const auto brightest = [&](bool left) {
        int best = 0;
        for (std::uint32_t y = 0; y < img.height; ++y) {
            for (std::uint32_t x = left ? 0 : img.width / 2; x < (left ? img.width / 2 : img.width); ++x) {
                best = std::max<int>(best, img.rgba[(std::size_t{y} * img.width + x) * 4]);
            }
        }
        return best;
    };
    CHECK(brightest(true) + 30 < brightest(false));
}

TEST_CASE("Vorschau: Punkte überlappen nicht, wenn x und y verschieden dicht sind", "[preview][spacing]") {
    // Spalten 4 Einheiten auseinander, Zeilen 1: Kabel läuft zeilenweise (Abstand 4), die nächste
    // Nachbarin liegt aber nur 1 entfernt. Die Punkte müssen nach dem kleineren Abstand dimensioniert sein.
    mapping::MatrixShape m{4, 4, {4.0F, 1.0F}, {}};
    CHECK(mapping::ledSpacing(m) == 1.0F);
    const mapping::LedMap map = bake(m);
    REQUIRE(map.spacing.size() == 1);
    CHECK(map.spacing[0] == 1.0F);
    const std::vector<ColorRgba> colors(16, kWhite);
    const auto image = renderPreview(map, colors);
    REQUIRE(image.ok());
    // Zwei übereinanderliegende Punkte lassen zwischen sich eine dunkle Zeile: die Mitte zwischen den
    // Zeilen 0 und 1 ist nicht weiß.
    const PixelPoint a = toPixel(image.value().frame, map.world[0]);
    const PixelPoint b = toPixel(image.value().frame, map.world[4]);
    const std::size_t x = static_cast<std::size_t>(a.x);
    const std::size_t y = static_cast<std::size_t>((a.y + b.y) / 2.0F);
    CHECK(image.value().rgba[(y * image.value().width + x) * 4] < 200);
}

TEST_CASE("Abstand: Formen", "[mapping][spacing]") {
    CHECK(mapping::ledSpacing(mapping::MatrixShape{8, 4, {2.0F, 0.5F}, {}}) == 0.5F);
    CHECK(mapping::ledSpacing(mapping::MatrixShape{8, 1, {2.0F, 0.5F}, {}}) == 2.0F);  // nur eine Zeile
    mapping::StripShape strip;
    strip.count = 10;
    strip.end = Vec3{5.0F, 0.0F, 0.0F};
    CHECK(mapping::ledSpacing(strip) == 0.5F);
    strip.count = 1;
    CHECK(mapping::ledSpacing(strip) == 0.0F);
    mapping::PointsShape points;
    points.points = {Vec3{0, 0, 0}, Vec3{3, 0, 0}, Vec3{3, 2, 0}};
    CHECK(mapping::ledSpacing(points) == 2.0F);
}
