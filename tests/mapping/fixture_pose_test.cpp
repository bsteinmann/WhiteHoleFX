// Tests für Ring, Säule und die Drehung von Fixtures (fixture_pose.cpp): Positionen, Boxen, Abstand und Prüfung.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "whitehole/mapping/fixture.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using Catch::Approx;

namespace {
void checkNear(const Vec3& v, float x, float y, float z) {
    CHECK(v.x == Approx(x).margin(1e-4));
    CHECK(v.y == Approx(y).margin(1e-4));
    CHECK(v.z == Approx(z).margin(1e-4));
}
}  // namespace

TEST_CASE("Ring: Positionen auf dem Kreis, erste LED oben, im Uhrzeigersinn", "[mapping][ring]") {
    RingShape ring;
    ring.count = 4;
    ring.radius = 2.0F;
    ring.startAngle = 90.0F;
    REQUIRE(validateShape(ring).ok());
    CHECK(ledCount(ring) == 4);
    const auto points = localPositions(ring);
    REQUIRE(points.size() == 4);
    checkNear(points[0], 2.0F, 4.0F, 0.0F);  // oben
    checkNear(points[1], 4.0F, 2.0F, 0.0F);  // rechts (Uhrzeigersinn)
    checkNear(points[2], 2.0F, 0.0F, 0.0F);  // unten
    checkNear(points[3], 0.0F, 2.0F, 0.0F);  // links
    const Box box = localBox(ring);
    CHECK(box.min.x == Approx(0.0F));
    CHECK(box.max.x == Approx(4.0F));
    CHECK(box.max.y == Approx(4.0F));
    CHECK(ledSpacing(ring) == Approx(2.0F * std::numbers::sqrt2_v<float>).epsilon(1e-3));
}

TEST_CASE("Ring: gegen den Uhrzeigersinn und Prüfung", "[mapping][ring]") {
    RingShape ring;
    ring.count = 4;
    ring.radius = 2.0F;
    ring.clockwise = false;
    const auto points = localPositions(ring);
    checkNear(points[1], 0.0F, 2.0F, 0.0F);  // links
    ring.count = 0;
    CHECK_FALSE(validateShape(ring).ok());
    ring.count = 4;
    ring.radius = 0.0F;
    CHECK_FALSE(validateShape(ring).ok());
    ring.radius = std::nanf("");
    CHECK_FALSE(validateShape(ring).ok());
}

TEST_CASE("Säule: Spirale von unten nach oben um die y-Achse", "[mapping][helix]") {
    HelixShape helix;
    helix.count = 5;
    helix.radius = 1.0F;
    helix.height = 8.0F;
    helix.turns = 1.0F;
    REQUIRE(validateShape(helix).ok());
    CHECK(ledCount(helix) == 5);
    const auto points = localPositions(helix);
    REQUIRE(points.size() == 5);
    float last = -1.0F;
    for (const Vec3& p : points) {
        CHECK(p.y > last);  // streng von unten nach oben
        last = p.y;
        const float dx = p.x - 1.0F;
        const float dz = p.z - 1.0F;
        CHECK(std::sqrt(dx * dx + dz * dz) == Approx(1.0F).epsilon(1e-4));  // auf dem Zylinder
    }
    const Box box = localBox(helix);
    CHECK(box.min.y >= -1e-4F);
    CHECK(box.max.y <= 8.0F + 1e-4F);
    CHECK(box.max.x == Approx(2.0F).margin(1e-4));
    helix.height = -1.0F;
    CHECK_FALSE(validateShape(helix).ok());
}

TEST_CASE("Drehung: 90 Grad um z tauscht die Achsen um die Mitte", "[mapping][rotation]") {
    Fixture f;
    f.name = "Strip";
    f.origin = Vec3{10.0F, 0.0F, 0.0F};
    f.shape = StripShape{2, Vec3{0.0F, 0.0F, 0.0F}, Vec3{4.0F, 0.0F, 0.0F}};  // LEDs bei x = 1 und 3
    const auto flat = worldPositions(f);
    checkNear(flat[0], 11.0F, 0.0F, 0.0F);
    checkNear(flat[1], 13.0F, 0.0F, 0.0F);
    f.rotation = Vec3{0.0F, 0.0F, 90.0F};
    const auto turned = worldPositions(f);
    // Mitte der lokalen Box (2, 0, 0) plus Ursprung = (12, 0, 0); die Reihe steht nun senkrecht.
    checkNear(turned[0], 12.0F, -1.0F, 0.0F);
    checkNear(turned[1], 12.0F, 1.0F, 0.0F);
    const Box box = worldBox(f);
    CHECK(box.min.x == Approx(12.0F).margin(1e-4));
    CHECK(box.max.x == Approx(12.0F).margin(1e-4));
    CHECK(box.min.y == Approx(-2.0F).margin(1e-4));
    CHECK(box.max.y == Approx(2.0F).margin(1e-4));
}

TEST_CASE("Drehung: um x und y, Reihenfolge x, y, z", "[mapping][rotation]") {
    checkNear(rotateAround(Vec3{1.0F, 0.0F, 0.0F}, Vec3{}, Vec3{0.0F, 0.0F, 90.0F}), 0.0F, 1.0F, 0.0F);
    checkNear(rotateAround(Vec3{0.0F, 1.0F, 0.0F}, Vec3{}, Vec3{90.0F, 0.0F, 0.0F}), 0.0F, 0.0F, 1.0F);
    checkNear(rotateAround(Vec3{0.0F, 0.0F, 1.0F}, Vec3{}, Vec3{0.0F, 90.0F, 0.0F}), 1.0F, 0.0F, 0.0F);
    // x zuerst, dann z: (0,1,0) -> x90 -> (0,0,1) -> z90 -> (0,0,1)
    checkNear(rotateAround(Vec3{0.0F, 1.0F, 0.0F}, Vec3{}, Vec3{90.0F, 0.0F, 90.0F}), 0.0F, 0.0F, 1.0F);
    // ohne Drehung bleibt der Punkt exakt
    CHECK(rotateAround(Vec3{3.0F, 4.0F, 5.0F}, Vec3{1.0F, 1.0F, 1.0F}, Vec3{}) == Vec3{3.0F, 4.0F, 5.0F});
}

TEST_CASE("Layout: rotate und setType, Fehler und Abtastung nutzen die Weltposition", "[mapping][rotation]") {
    Layout layout;
    const auto id = layout.add("S", Vec3{}, StripShape{2, Vec3{}, Vec3{4.0F, 0.0F, 0.0F}});
    REQUIRE(id.ok());
    CHECK(layout.rotate(id.value(), Vec3{0.0F, 0.0F, 90.0F}).ok());
    CHECK(layout.find(id.value())->rotation == Vec3{0.0F, 0.0F, 90.0F});
    CHECK(layout.rotate(FixtureId{99}, Vec3{}).error().code == ErrorCode::NotFound);
    CHECK_FALSE(layout.rotate(id.value(), Vec3{std::nanf(""), 0.0F, 0.0F}).ok());
    CHECK(layout.setType(id.value(), "Bauform").ok());
    CHECK(layout.find(id.value())->type == "Bauform");
    CHECK(layout.setType(FixtureId{99}, "x").error().code == ErrorCode::NotFound);
}

TEST_CASE("Säule (Röhre): Ringe übereinander, Kabel Ring für Ring, jeder zweite Ring rückwärts", "[mapping][cylinder]") {
    CylinderShape c;
    c.perRing = 4;
    c.rings = 2;
    c.radius = 2.0F;
    c.height = 10.0F;
    c.startAngle = 0.0F;
    c.clockwise = false;  // gegen den Uhrzeigersinn: +x, dann +z (x-z-Ebene)
    REQUIRE(validateShape(c).ok());
    CHECK(ledCount(c) == 8);
    const auto points = localPositions(c);
    REQUIRE(points.size() == 8);
    // Unterer Ring (y = 2,5): vorwärts
    checkNear(points[0], 4.0F, 2.5F, 2.0F);
    checkNear(points[1], 2.0F, 2.5F, 4.0F);
    checkNear(points[2], 0.0F, 2.5F, 2.0F);
    checkNear(points[3], 2.0F, 2.5F, 0.0F);
    // Oberer Ring (y = 7,5): rückwärts, beginnt dort, wo der untere endete
    checkNear(points[4], 2.0F, 7.5F, 0.0F);
    checkNear(points[7], 4.0F, 7.5F, 2.0F);
    const Box box = localBox(c);
    CHECK(box.max.x == Approx(4.0F));
    CHECK(box.max.y == Approx(10.0F));
    CHECK(box.max.z == Approx(4.0F));

    c.serpentine = false;  // alle Ringe vorwärts
    checkNear(localPositions(c)[4], 4.0F, 7.5F, 2.0F);
}

TEST_CASE("Säule (Röhre): Abstand und Prüfung", "[mapping][cylinder]") {
    CylinderShape c;
    c.perRing = 12;
    c.rings = 20;
    c.radius = 3.0F;
    c.height = 20.0F;
    CHECK(ledSpacing(c) == Approx(1.0F).epsilon(1e-3));  // Ringabstand 1, Sehne 2*3*sin(15°) = 1,55
    c.perRing = 0;
    CHECK_FALSE(validateShape(c).ok());
    c.perRing = 12;
    c.rings = 0;
    CHECK_FALSE(validateShape(c).ok());
    c.rings = 20;
    c.height = 0.0F;
    CHECK_FALSE(validateShape(c).ok());
    c.height = 20.0F;
    c.perRing = 4000000;
    c.rings = 4000000;
    CHECK_FALSE(validateShape(c).ok());  // zu viele LEDs
}

// ---- Drehung um eine feste Weltachse (Drehringe der Bühnenansicht) ----

TEST_CASE("Weltdrehung: ohne Vorbelegung ändert sich nur der Winkel der Achse", "[mapping][pose]") {
    const Vec3 x = rotationAfterWorldTurn({0.0F, 0.0F, 0.0F}, 0, 30.0F);
    CHECK(x.x == Catch::Approx(30.0F).margin(1e-3));
    CHECK(x.y == Catch::Approx(0.0F).margin(1e-3));
    CHECK(x.z == Catch::Approx(0.0F).margin(1e-3));
    const Vec3 z = rotationAfterWorldTurn({0.0F, 0.0F, 20.0F}, 2, 15.0F);
    CHECK(z.z == Catch::Approx(35.0F).margin(1e-3));
    CHECK(rotationAfterWorldTurn({0.0F, 0.0F, 0.0F}, 1, 190.0F).y != 190.0F);  // landet in (-180, 180]
}

TEST_CASE("Weltdrehung: die Lage im Raum ist genau um die Weltdrehung gedreht", "[mapping][pose]") {
    const Vec3 zero{0.0F, 0.0F, 0.0F};
    const Vec3 start[] = {{10.0F, 20.0F, 30.0F}, {-45.0F, 80.0F, 120.0F}, {170.0F, -30.0F, -100.0F}, {0.0F, 90.0F, 25.0F}};
    for (const Vec3& before : start) {
        for (int axis = 0; axis < 3; ++axis) {
            for (const float turn : {5.0F, -17.0F, 90.0F}) {
                const Vec3 after = rotationAfterWorldTurn(before, axis, turn);
                Vec3 world{};
                (axis == 0 ? world.x : axis == 1 ? world.y : world.z) = turn;
                for (const Vec3& p : {Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.3F, -0.7F, 0.5F}}) {
                    const Vec3 expected = rotateAround(rotateAround(p, zero, before), zero, world);
                    const Vec3 got = rotateAround(p, zero, after);
                    CHECK(got.x == Catch::Approx(expected.x).margin(1e-3));
                    CHECK(got.y == Catch::Approx(expected.y).margin(1e-3));
                    CHECK(got.z == Catch::Approx(expected.z).margin(1e-3));
                }
            }
        }
    }
}
