// Tests: Achsen-Gizmo der 3D-Bühnenansicht (Geometrie, Treffer, Ziehen, Einrasten).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "whitehole/mapping/led_map.hpp"
#include "whitehole/preview/gizmo.hpp"

using namespace whitehole;
using namespace whitehole::preview;
using Catch::Approx;

namespace {

mapping::LedMap boxMap() {
    mapping::LedMap map;
    for (const float x : {0.0F, 10.0F}) {
        for (const float y : {0.0F, 10.0F}) {
            for (const float z : {0.0F, 10.0F}) {
                map.world.push_back({x, y, z});
            }
        }
    }
    map.unit = map.world;
    return map;
}

GizmoGeometry gizmoIn(ViewPreset preset, const Vec3& anchor) {
    const mapping::LedMap map = boxMap();
    const Projector projector{cameraFor(preset), boundsOf(map), 400, 300, 24};
    const auto gizmo = gizmoFor(projector, anchor);
    REQUIRE(gizmo.has_value());
    return *gizmo;
}

}  // namespace

TEST_CASE("Gizmo: von vorn zeigt x nach rechts, y nach oben, z zur Kamera (nicht greifbar)", "[preview][gizmo]") {
    const GizmoGeometry g = gizmoIn(ViewPreset::Front, {2.0F, 3.0F, 0.0F});
    CHECK(g.tipX[0] > g.originX);
    CHECK(g.tipY[0] == Approx(g.originY).margin(0.01));
    CHECK(g.tipY[1] < g.originY);  // Zeile 0 oben: y nach oben = kleinere Zeile
    CHECK(gizmoAxisPixels(g, 0) == Approx(kGizmoLengthPixels).margin(0.5));
    CHECK(gizmoAxisPixels(g, 1) == Approx(kGizmoLengthPixels).margin(0.5));
    CHECK(gizmoAxisPixels(g, 2) < kGizmoMinPixels);
    CHECK_FALSE(pickGizmoAxis(g, g.originX + 3.0F, g.originY + 3.0F, 1.0F).has_value());
}

TEST_CASE("Gizmo: Treffer wählt die nächste Linie und sonst nichts", "[preview][gizmo]") {
    const GizmoGeometry g = gizmoIn(ViewPreset::Front, {2.0F, 3.0F, 0.0F});
    const float midX = (g.originX + g.tipX[0]) / 2.0F;
    CHECK(pickGizmoAxis(g, midX, g.originY + 3.0F) == std::optional<int>{0});
    const float midY = (g.originY + g.tipY[1]) / 2.0F;
    CHECK(pickGizmoAxis(g, g.originX - 2.0F, midY) == std::optional<int>{1});
    CHECK_FALSE(pickGizmoAxis(g, midX, g.originY + 40.0F).has_value());
    // Hinter der Spitze zählt noch die Spitze, weit dahinter nicht
    CHECK(pickGizmoAxis(g, g.tipX[0] + 4.0F, g.tipY[0]) == std::optional<int>{0});
    CHECK_FALSE(pickGizmoAxis(g, g.tipX[0] + 30.0F, g.tipY[0]).has_value());
}

TEST_CASE("Gizmo: Ziehen entlang der Achse rechnet Bildpunkte in Welteinheiten um", "[preview][gizmo]") {
    const GizmoGeometry g = gizmoIn(ViewPreset::Front, {2.0F, 3.0F, 0.0F});
    // Ein Pfeil (70 Bildpunkte) ist worldLength Einheiten lang
    CHECK(gizmoDragDistance(g, 0, kGizmoLengthPixels, 0.0F) == Approx(g.worldLength).margin(0.01));
    CHECK(gizmoDragDistance(g, 0, -kGizmoLengthPixels / 2.0F, 0.0F) == Approx(-g.worldLength / 2.0F).margin(0.01));
    CHECK(gizmoDragDistance(g, 0, 0.0F, 50.0F) == Approx(0.0F).margin(0.01));      // quer zur Achse: nichts
    CHECK(gizmoDragDistance(g, 1, 0.0F, -kGizmoLengthPixels) == Approx(g.worldLength).margin(0.01));  // nach oben = +y
    CHECK(gizmoDragDistance(g, 2, 30.0F, 30.0F) == 0.0F);                          // z zeigt zur Kamera
}

TEST_CASE("Gizmo: in der Perspektive sind alle drei Achsen greifbar", "[preview][gizmo]") {
    const GizmoGeometry g = gizmoIn(ViewPreset::Perspective, {5.0F, 5.0F, 5.0F});
    for (int axis = 0; axis < 3; ++axis) {
        CHECK(gizmoAxisPixels(g, axis) >= kGizmoMinPixels);
    }
    const float midX = (g.originX + g.tipX[2]) / 2.0F;
    const float midY = (g.originY + g.tipY[2]) / 2.0F;
    CHECK(pickGizmoAxis(g, midX, midY) == std::optional<int>{2});
}

TEST_CASE("Gizmo: Einrasten", "[preview][gizmo]") {
    CHECK(snapDistance(0.34F, 0.1F) == Approx(0.3F));
    CHECK(snapDistance(-0.76F, 0.1F) == Approx(-0.8F));
    CHECK(snapDistance(0.34F, 0.0F) == Approx(0.34F));
}

TEST_CASE("Gizmo: bei sehr kleinem Bild verkürzt sich der Pfeil, statt zu verschwinden", "[preview][gizmo]") {
    const mapping::LedMap map = boxMap();
    const Projector projector{cameraFor(ViewPreset::Perspective), boundsOf(map), 100, 64, 24};
    const auto gizmo = gizmoFor(projector, {0.0F, 0.0F, 0.0F});
    REQUIRE(gizmo.has_value());
    CHECK(gizmoAxisPixels(*gizmo, 0) > 0.0F);
}

// ---- Drehringe ----

TEST_CASE("Gizmo: Ringe sind Kreise um die Achsen und von der richtigen Seite greifbar", "[preview][gizmo]") {
    // Von vorn: Ring um z ist ein Kreis (sichtbar), Ringe um x und y sind Kanten (nicht greifbar).
    const GizmoGeometry g = gizmoIn(ViewPreset::Front, {2.0F, 3.0F, 0.0F});
    REQUIRE(g.ringVisible[2]);
    const float radius = kGizmoLengthPixels * kGizmoRingRadius;
    CHECK(gizmoRingMinorPixels(g, 2) == Approx(radius).margin(radius * 0.05F));
    CHECK(gizmoRingMinorPixels(g, 0) < kGizmoRingMinMinorPixels);
    CHECK(gizmoRingMinorPixels(g, 1) < kGizmoRingMinMinorPixels);
    // Punkt auf dem Kreis (rechts der Mitte) trifft den z-Ring, die Mitte und weit außen nichts.
    CHECK(pickGizmoRing(g, g.originX + radius, g.originY) == std::optional<int>{2});
    CHECK_FALSE(pickGizmoRing(g, g.originX, g.originY).has_value());
    CHECK_FALSE(pickGizmoRing(g, g.originX + radius * 2.0F, g.originY).has_value());
}

TEST_CASE("Gizmo: Ringwinkel läuft im Rechtssinn um die Achse", "[preview][gizmo]") {
    const GizmoGeometry g = gizmoIn(ViewPreset::Front, {0.0F, 0.0F, 0.0F});
    const float radius = kGizmoLengthPixels * kGizmoRingRadius;
    // z-Ring: 0° liegt auf x (rechts), 90° auf y (im Bild oben, Zeile 0 oben, also kleineres y).
    CHECK(gizmoRingAngle(g, 2, g.originX + radius, g.originY) == Approx(0.0F).margin(2.0F));
    CHECK(gizmoRingAngle(g, 2, g.originX, g.originY - radius) == Approx(90.0F).margin(2.0F));
    CHECK(gizmoRingAngle(g, 2, g.originX - radius, g.originY) == Approx(180.0F).margin(2.0F));
    CHECK(gizmoRingAngle(g, 2, g.originX, g.originY + radius) == Approx(270.0F).margin(2.0F));
}

TEST_CASE("Gizmo: Winkelunterschied geht kurz über die Nullgrenze", "[preview][gizmo]") {
    CHECK(angleDelta(350.0F, 10.0F) == Approx(20.0F));
    CHECK(angleDelta(10.0F, 350.0F) == Approx(-20.0F));
    CHECK(angleDelta(0.0F, 90.0F) == Approx(90.0F));
    CHECK(angleDelta(0.0F, 180.0F) == Approx(180.0F));
}
