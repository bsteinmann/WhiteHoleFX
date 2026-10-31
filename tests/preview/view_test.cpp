// Tests: Ausschnitt der Vorschau, Umrechnung Welt <-> Bild, Treffertests.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/preview/preview.hpp"

using namespace whitehole;
using namespace whitehole::preview;
using Catch::Approx;

namespace {

mapping::MatrixShape matrix(std::uint32_t columns, std::uint32_t rows) {
    mapping::MatrixShape m;
    m.columns = columns;
    m.rows = rows;
    return m;
}

struct Scene {
    mapping::Layout layout;
    mapping::FixtureId left;
    mapping::FixtureId right;

    Scene() {
        left = layout.add("Links", Vec3{0.0F, 0.0F, 0.0F}, matrix(8, 8)).value();
        right = layout.add("Rechts", Vec3{20.0F, 0.0F, 0.0F}, matrix(8, 4)).value();
    }
};

Image render(const mapping::Layout& layout, const PreviewOptions& options = {}) {
    const mapping::LedMap map = layout.bake().value();
    return renderPreview(map, layoutColors(map), options).value();
}

}  // namespace

TEST_CASE("Ansicht: toPixel und toWorld sind Umkehrungen", "[preview][view]") {
    Scene scene;
    const Image image = render(scene.layout);
    const Vec3 world{12.5F, 3.25F, 0.0F};
    const PixelPoint p = toPixel(image.frame, world);
    const Vec3 back = toWorld(image.frame, p.x, p.y);
    CHECK(back.x == Approx(world.x).margin(1e-3));
    CHECK(back.y == Approx(world.y).margin(1e-3));
}

TEST_CASE("Ansicht: der gemeldete Rahmen passt zum Bild", "[preview][view]") {
    Scene scene;
    const Image image = render(scene.layout);
    // Die linke untere Ecke der LED-Mitten liegt links unten im Bild, die obere rechte oben rechts.
    const PixelPoint low = toPixel(image.frame, Vec3{0.5F, 0.5F, 0.0F});
    const PixelPoint high = toPixel(image.frame, Vec3{27.5F, 7.5F, 0.0F});
    CHECK(low.x < high.x);
    CHECK(low.y > high.y);
    CHECK(low.x >= 0.0F);
    CHECK(high.x <= static_cast<float>(image.width));
    CHECK(low.y <= static_cast<float>(image.height));
    CHECK(high.y >= 0.0F);
}

TEST_CASE("Ansicht: ein vorgegebener Rahmen bleibt erhalten", "[preview][view]") {
    Scene scene;
    const Image first = render(scene.layout);
    REQUIRE(scene.layout.move(scene.right, Vec3{60.0F, 0.0F, 0.0F}).ok());  // Box wird breiter
    const Image refit = render(scene.layout);
    CHECK_FALSE(refit.frame == first.frame);  // ohne Vorgabe passt sich der Ausschnitt an

    PreviewOptions fixed;
    fixed.frame = first.frame;
    const Image held = render(scene.layout, fixed);
    CHECK(held.frame == first.frame);
}

TEST_CASE("Ansicht: ungültiger Rahmen wird ignoriert", "[preview][view]") {
    Scene scene;
    PreviewOptions options;
    ViewFrame bad;
    bad.scale = 0.0F;
    options.frame = bad;
    const Image image = render(scene.layout, options);
    CHECK(image.frame.scale > 0.0F);
}

TEST_CASE("Ansicht: Fixture unter dem Mauszeiger", "[preview][view]") {
    Scene scene;
    const Image image = render(scene.layout);
    const PixelPoint l = toPixel(image.frame, Vec3{4.0F, 4.0F, 0.0F});
    const PixelPoint r = toPixel(image.frame, Vec3{24.0F, 2.0F, 0.0F});
    CHECK(fixtureAt(scene.layout, image.frame, l.x, l.y) == scene.left);
    CHECK(fixtureAt(scene.layout, image.frame, r.x, r.y) == scene.right);
    const PixelPoint gap = toPixel(image.frame, Vec3{14.0F, 4.0F, 0.0F});
    CHECK_FALSE(fixtureAt(scene.layout, image.frame, gap.x, gap.y, 0.0F).has_value());
}

TEST_CASE("Ansicht: bei Überlappung gewinnt das kleinere Fixture", "[preview][view]") {
    mapping::Layout layout;
    const auto big = layout.add("Gross", Vec3{}, matrix(20, 20)).value();
    const auto small = layout.add("Klein", Vec3{5.0F, 5.0F, 0.0F}, matrix(4, 4)).value();
    const mapping::LedMap map = layout.bake().value();
    const Image image = renderPreview(map, layoutColors(map)).value();
    const PixelPoint inside = toPixel(image.frame, Vec3{7.0F, 7.0F, 0.0F});
    const PixelPoint outside = toPixel(image.frame, Vec3{15.0F, 15.0F, 0.0F});
    CHECK(fixtureAt(layout, image.frame, inside.x, inside.y) == small);
    CHECK(fixtureAt(layout, image.frame, outside.x, outside.y) == big);
}

TEST_CASE("Ansicht: Gummiband wählt berührte Fixtures", "[preview][view]") {
    Scene scene;
    const Image image = render(scene.layout);
    const PixelPoint a = toPixel(image.frame, Vec3{-1.0F, -1.0F, 0.0F});
    const PixelPoint b = toPixel(image.frame, Vec3{10.0F, 9.0F, 0.0F});
    const PixelRect onlyLeft{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
    const auto one = fixturesInRect(scene.layout, image.frame, onlyLeft);
    REQUIRE(one.size() == 1);
    CHECK(one.front() == scene.left);

    const PixelRect everything{0.0F, 0.0F, static_cast<float>(image.width), static_cast<float>(image.height)};
    CHECK(fixturesInRect(scene.layout, image.frame, everything).size() == 2);
}

TEST_CASE("Ansicht: Weltbox eines Fixtures", "[preview][view]") {
    Scene scene;
    const mapping::Box b = worldBox(*scene.layout.find(scene.right));
    CHECK(b.min.x == Approx(20.0F));
    CHECK(b.max.x == Approx(28.0F));
    CHECK(b.max.y == Approx(4.0F));
}
