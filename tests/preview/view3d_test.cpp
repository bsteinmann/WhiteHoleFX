// Tests: Kamera und Projektion der 3D-Vorschau (Presets, Orbit, Zoom, Tiefe, Zeichnen).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <span>
#include <vector>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/preview/view3d.hpp"

using namespace whitehole;
using namespace whitehole::preview;
using Catch::Approx;

namespace {

/// Karte aus freien Punkten (keine Fixtures nötig: die 3D-Ansicht kennt nur Positionen).
mapping::LedMap pointsMap(std::vector<Vec3> points) {
    mapping::LedMap map;
    map.world = std::move(points);
    map.unit = map.world;
    return map;
}

mapping::LedMap cubeMap() {
    mapping::LedMap map;
    for (int z = 0; z < 4; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                map.world.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
            }
        }
    }
    map.unit = map.world;
    return map;
}

Projector projectorFor(const mapping::LedMap& map, const Camera3d& camera, std::uint32_t w = 400, std::uint32_t h = 300) {
    const Bounds3d b = boundsOf(map);
    return Projector{camera, b, w, h, 24};
}

std::array<std::uint8_t, 3> pixelAt(const Image& image, int x, int y) {
    const std::size_t i = (static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4;
    return {image.rgba[i], image.rgba[i + 1], image.rgba[i + 2]};
}

}  // namespace

TEST_CASE("3D: Front zeigt x nach rechts und y nach oben, die Mitte liegt in der Bildmitte", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const Projector p = projectorFor(map, cameraFor(ViewPreset::Front));
    const ScreenPoint centre = p.project(Vec3{1.5F, 1.5F, 1.5F}).value();
    CHECK(centre.x == Approx(200.0F).margin(1e-3));
    CHECK(centre.y == Approx(150.0F).margin(1e-3));
    const ScreenPoint right = p.project(Vec3{3.0F, 1.5F, 1.5F}).value();
    const ScreenPoint up = p.project(Vec3{1.5F, 3.0F, 1.5F}).value();
    CHECK(right.x > centre.x);
    CHECK(right.y == Approx(centre.y).margin(1e-3));
    CHECK(up.y < centre.y);
    CHECK(up.x == Approx(centre.x).margin(1e-3));
}

TEST_CASE("3D: Front ist eine Parallelprojektion, z ändert das Bild nicht", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const Projector p = projectorFor(map, cameraFor(ViewPreset::Front));
    const ScreenPoint a = p.project(Vec3{2.0F, 1.0F, 0.0F}).value();
    const ScreenPoint b = p.project(Vec3{2.0F, 1.0F, 3.0F}).value();
    CHECK(a.x == Approx(b.x).margin(1e-3));
    CHECK(a.y == Approx(b.y).margin(1e-3));
    CHECK(a.depth > b.depth);  // z zeigt zum Betrachter: der Punkt mit größerem z ist näher
}

TEST_CASE("3D: Seite blickt von rechts, Oben blickt von oben", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const Projector side = projectorFor(map, cameraFor(ViewPreset::Side));
    // Von rechts gesehen liegt die vordere Seite (größeres z) links im Bild.
    CHECK(side.project(Vec3{1.5F, 1.5F, 3.0F})->x < side.project(Vec3{1.5F, 1.5F, 0.0F})->x);
    CHECK(side.project(Vec3{1.5F, 3.0F, 1.5F})->y < side.project(Vec3{1.5F, 0.0F, 1.5F})->y);

    const Projector top = projectorFor(map, cameraFor(ViewPreset::Top));
    // Von oben gesehen liegt die Vorderseite (größeres z) unten im Bild, y ändert die Lage nicht.
    CHECK(top.project(Vec3{1.5F, 1.5F, 3.0F})->y > top.project(Vec3{1.5F, 1.5F, 0.0F})->y);
    CHECK(top.project(Vec3{1.5F, 3.0F, 1.5F})->y == Approx(top.project(Vec3{1.5F, 0.0F, 1.5F})->y).margin(1e-3));
    CHECK(top.project(Vec3{3.0F, 1.5F, 1.5F})->x > top.project(Vec3{0.0F, 1.5F, 1.5F})->x);
}

TEST_CASE("3D: Perspektive macht nahe Punkte größer", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const Projector p = projectorFor(map, cameraFor(ViewPreset::Perspective));
    const ScreenPoint near = p.project(Vec3{1.5F, 1.5F, 3.0F}).value();
    const ScreenPoint far = p.project(Vec3{1.5F, 1.5F, 0.0F}).value();
    // Die Kamera steht bei yaw 35 / pitch 22 vorn rechts oben: z = 3 ist näher als z = 0.
    CHECK(near.depth < far.depth);
    CHECK(near.pixelsPerUnit > far.pixelsPerUnit);
}

TEST_CASE("3D: alle Ecken der Installation liegen im Bild, in jeder Ansicht", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    for (const ViewPreset preset : {ViewPreset::Front, ViewPreset::Side, ViewPreset::Top, ViewPreset::Perspective}) {
        const Projector p = projectorFor(map, cameraFor(preset));
        for (const Vec3& led : map.world) {
            const ScreenPoint s = p.project(led).value();
            CHECK(s.x >= 0.0F);
            CHECK(s.x <= 400.0F);
            CHECK(s.y >= 0.0F);
            CHECK(s.y <= 300.0F);
        }
    }
}

TEST_CASE("3D: Orbit hält Yaw im Bereich und begrenzt Pitch", "[preview][view3d]") {
    Camera3d camera = cameraFor(ViewPreset::Perspective);
    camera = orbited(camera, 400.0F, 500.0F);
    CHECK(camera.yawDegrees > -180.0F);
    CHECK(camera.yawDegrees <= 180.0F);
    CHECK(camera.pitchDegrees == Approx(90.0F));
    camera = orbited(camera, -10.0F, -1000.0F);
    CHECK(camera.pitchDegrees == Approx(-90.0F));
    // Ungültige Eingaben ändern nichts.
    const Camera3d same = orbited(camera, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
    CHECK(same == camera);
}

TEST_CASE("3D: Zoom bleibt im erlaubten Bereich, Verschieben ebenso", "[preview][view3d]") {
    Camera3d camera;
    CHECK(zoomed(camera, 1000.0F).zoom == Approx(kMaxZoom3d));
    CHECK(zoomed(camera, 0.0001F).zoom == Approx(kMinZoom3d));
    CHECK(zoomed(camera, -2.0F).zoom == Approx(1.0F));
    CHECK(panned(camera, 100.0F, -100.0F).panX == Approx(4.0F));
    CHECK(panned(camera, 100.0F, -100.0F).panY == Approx(-4.0F));
}

TEST_CASE("3D: Zoom vergrößert, Verschieben verschiebt das Bild", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const Camera3d base = cameraFor(ViewPreset::Front);
    const ScreenPoint a = projectorFor(map, base).project(Vec3{3.0F, 1.5F, 1.5F}).value();
    const ScreenPoint zoomedIn = projectorFor(map, zoomed(base, 2.0F)).project(Vec3{3.0F, 1.5F, 1.5F}).value();
    CHECK(zoomedIn.x - 200.0F == Approx(2.0F * (a.x - 200.0F)).margin(1e-2));
    const ScreenPoint moved = projectorFor(map, panned(base, 0.1F, 0.1F)).project(Vec3{3.0F, 1.5F, 1.5F}).value();
    CHECK(moved.x == Approx(a.x + 30.0F).margin(1e-2));  // 0,1 der kürzeren Seite (300)
    CHECK(moved.y == Approx(a.y - 30.0F).margin(1e-2));
}

TEST_CASE("3D: ein Punkt hinter der Kamera wird nicht projiziert", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const Projector p = projectorFor(map, cameraFor(ViewPreset::Perspective));
    CHECK_FALSE(p.project(Vec3{1.5F, 1.5F, 1000.0F}).has_value());
}

TEST_CASE("3D: renderPreview3d zeichnet Farben, die nahe LED verdeckt die ferne", "[preview][view3d]") {
    // Zwei LEDs auf einer Blicklinie der Frontansicht: vorn grün, hinten rot.
    const mapping::LedMap map = pointsMap({Vec3{0.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 5.0F}});
    const std::vector<ColorRgba> colors{{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}};
    PreviewOptions options;
    options.width = 200;
    options.height = 200;
    Camera3d camera = cameraFor(ViewPreset::Front);
    camera.showBox = false;
    const Image image = renderPreview3d(map, colors, camera, options).value();
    REQUIRE(image.width == 200);
    REQUIRE(image.height == 200);
    const auto px = pixelAt(image, 100, 100);
    CHECK(px[1] > 200);
    CHECK(px[0] < 100);
}

TEST_CASE("3D: eine LED erscheint dort, wo der Projector sie hinlegt", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    std::vector<ColorRgba> colors(map.world.size(), ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    const std::size_t lit = 3 + 4 * 2 + 16 * 1;  // (3, 2, 1)
    colors[lit] = ColorRgba{1.0F, 1.0F, 1.0F, 1.0F};
    PreviewOptions options;
    options.width = 400;
    options.height = 300;
    Camera3d camera = cameraFor(ViewPreset::Perspective);
    camera.showBox = false;
    const Image image = renderPreview3d(map, colors, camera, options).value();
    const Bounds3d b = boundsOf(map);
    const ScreenPoint s = Projector{camera, b, 400, 300, options.margin}.project(map.world[lit]).value();
    const auto px = pixelAt(image, static_cast<int>(s.x), static_cast<int>(s.y));
    CHECK(px[0] > 150);
    CHECK(px[1] > 150);
    CHECK(px[2] > 150);
}

TEST_CASE("3D: der Rahmen wird gezeichnet und lässt sich abschalten", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const std::vector<ColorRgba> colors(map.world.size(), ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    Camera3d with = cameraFor(ViewPreset::Perspective);
    Camera3d without = with;
    without.showBox = false;
    const Image a = renderPreview3d(map, colors, with).value();
    const Image b = renderPreview3d(map, colors, without).value();
    CHECK(a.rgba != b.rgba);
}

TEST_CASE("3D: ungültige Eingaben ergeben Fehler", "[preview][view3d]") {
    const mapping::LedMap map = cubeMap();
    const std::vector<ColorRgba> colors(map.world.size(), ColorRgba{});
    CHECK_FALSE(renderPreview3d(mapping::LedMap{}, {}, Camera3d{}).ok());
    CHECK_FALSE(renderPreview3d(map, std::span<const ColorRgba>{colors}.first(3), Camera3d{}).ok());
    PreviewOptions bad;
    bad.width = 0;
    CHECK_FALSE(renderPreview3d(map, colors, Camera3d{}, bad).ok());
    bad.width = 40;
    bad.margin = 30;
    CHECK_FALSE(renderPreview3d(map, colors, Camera3d{}, bad).ok());
}

TEST_CASE("3D: eine einzelne LED und flache Karten brechen die Rechnung nicht", "[preview][view3d]") {
    PreviewOptions options;
    const std::vector<ColorRgba> one{{1.0F, 0.5F, 0.0F, 1.0F}};
    CHECK(renderPreview3d(pointsMap({Vec3{3.0F, 3.0F, 3.0F}}), one, Camera3d{}, options).ok());
    // Flache Matrix von oben gesehen: alle LEDs auf einer Linie, keine Division durch 0.
    mapping::Layout layout;
    mapping::MatrixShape shape;
    shape.columns = 8;
    shape.rows = 8;
    REQUIRE(layout.add("M", Vec3{}, shape).ok());
    const mapping::LedMap flat = layout.bake().value();
    const std::vector<ColorRgba> colors(flat.world.size(), ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    for (const ViewPreset preset : {ViewPreset::Front, ViewPreset::Side, ViewPreset::Top, ViewPreset::Perspective}) {
        CHECK(renderPreview3d(flat, colors, cameraFor(preset), options).ok());
    }
}

TEST_CASE("3D: feste Ansichten füllen das Bild mit der Installation", "[preview][view3d]") {
    // Ein langer flacher Streifen (hängender Strip, von vorn gesehen) soll die Bildbreite nutzen.
    mapping::LedMap strip;
    for (int i = 0; i < 8; ++i) {
        strip.world.push_back({static_cast<float>(i), 0.0F, 0.0F});
    }
    strip.unit = strip.world;
    const Projector p = projectorFor(strip, cameraFor(ViewPreset::Front));
    const float width = p.project(strip.world.back())->x - p.project(strip.world.front())->x;
    CHECK(width > 300.0F);
    CHECK(width < 352.0F);  // innerhalb der Ränder
}
