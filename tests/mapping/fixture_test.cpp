// Fixtures: Prüfung der Eingaben, LED-Anzahl, Boxen und Positionen.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "whitehole/mapping/fixture.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using Catch::Approx;

namespace {
void checkNear(const Vec3& v, float x, float y, float z) {
    CHECK(v.x == Approx(x).margin(1e-5));
    CHECK(v.y == Approx(y).margin(1e-5));
    CHECK(v.z == Approx(z).margin(1e-5));
}
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();
}  // namespace

TEST_CASE("validateShape lehnt ungültige Matrizen ab") {
    MatrixShape m;
    CHECK(validateShape(m).ok());
    m.columns = 0;
    CHECK_FALSE(validateShape(m).ok());
    m = MatrixShape{};
    m.rows = 0;
    CHECK_FALSE(validateShape(m).ok());
    m = MatrixShape{};
    m.pitch = Vec2{0.0F, 1.0F};
    CHECK_FALSE(validateShape(m).ok());
    m.pitch = Vec2{1.0F, -2.0F};
    CHECK_FALSE(validateShape(m).ok());
    m.pitch = Vec2{kNaN, 1.0F};
    CHECK_FALSE(validateShape(m).ok());
    m.pitch = Vec2{1.0F, kInf};
    CHECK_FALSE(validateShape(m).ok());

    m = MatrixShape{};
    m.columns = 65536;
    m.rows = 65536;  // 4 Milliarden LEDs: abgelehnt, ohne zu überlaufen
    const auto huge = validateShape(m);
    REQUIRE_FALSE(huge.ok());
    CHECK(huge.error().code == ErrorCode::InvalidArgument);

    m.columns = 2048;
    m.rows = 2048;  // genau die Obergrenze
    CHECK(validateShape(m).ok());
    m.rows = 2049;
    CHECK_FALSE(validateShape(m).ok());
}

TEST_CASE("validateShape lehnt ungültige Strips, Gitter und Punktwolken ab") {
    StripShape s;
    CHECK(validateShape(s).ok());
    s.count = 0;
    CHECK_FALSE(validateShape(s).ok());
    s.count = 4;
    s.end = Vec3{kNaN, 0.0F, 0.0F};
    CHECK_FALSE(validateShape(s).ok());

    GridShape g;
    CHECK(validateShape(g).ok());
    g.ny = 0;
    CHECK_FALSE(validateShape(g).ok());
    g = GridShape{};
    g.pitch.z = 0.0F;
    CHECK_FALSE(validateShape(g).ok());
    g = GridShape{};
    g.nx = g.ny = g.nz = 1000;  // 10^9
    CHECK_FALSE(validateShape(g).ok());
    g.nx = g.ny = g.nz = 161;  // 4.17 Mio.: knapp erlaubt
    CHECK(validateShape(g).ok());

    PointsShape p;
    CHECK_FALSE(validateShape(p).ok());  // leer
    p.points = {Vec3{0, 0, 0}, Vec3{1, kInf, 0}};
    CHECK_FALSE(validateShape(p).ok());
    p.points = {Vec3{0, 0, 0}, Vec3{1, 2, 3}};
    CHECK(validateShape(p).ok());
}

TEST_CASE("Matrix: Positionen liegen in Zellmitten, Zeile 0 oben") {
    MatrixShape m;
    m.columns = 4;
    m.rows = 2;
    m.pitch = Vec2{2.0F, 3.0F};
    const FixtureShape shape = m;

    CHECK(ledCount(shape) == 8U);
    CHECK(localBox(shape) == Box{Vec3{0, 0, 0}, Vec3{8, 6, 0}});

    const auto positions = localPositions(shape);
    REQUIRE(positions.size() == 8U);
    checkNear(positions[0], 1.0F, 4.5F, 0.0F);  // links oben
    checkNear(positions[3], 7.0F, 4.5F, 0.0F);  // rechts oben
    checkNear(positions[4], 1.0F, 1.5F, 0.0F);  // links unten
    checkNear(positions[7], 7.0F, 1.5F, 0.0F);
}

TEST_CASE("Matrix: Verkabelung bestimmt die Reihenfolge der Positionen") {
    MatrixShape m;
    m.columns = 3;
    m.rows = 2;
    m.wiring.serpentine = true;
    m.wiring.start = StartCorner::BottomLeft;
    const auto positions = localPositions(m);
    REQUIRE(positions.size() == 6U);
    checkNear(positions[0], 0.5F, 0.5F, 0.0F);  // unten links
    checkNear(positions[2], 2.5F, 0.5F, 0.0F);
    checkNear(positions[3], 2.5F, 1.5F, 0.0F);  // Umkehr in der oberen Zeile
    checkNear(positions[5], 0.5F, 1.5F, 0.0F);
}

TEST_CASE("Strip: LEDs sitzen in der Mitte gleich langer Abschnitte") {
    StripShape s;
    s.count = 4;
    s.start = Vec3{0, 0, 0};
    s.end = Vec3{8, 4, -2};
    const FixtureShape shape = s;

    CHECK(ledCount(shape) == 4U);
    CHECK(localBox(shape) == Box{Vec3{0, 0, -2}, Vec3{8, 4, 0}});
    const auto positions = localPositions(shape);
    REQUIRE(positions.size() == 4U);
    checkNear(positions[0], 1.0F, 0.5F, -0.25F);
    checkNear(positions[3], 7.0F, 3.5F, -1.75F);
}

TEST_CASE("Gitter: y = 0 ist unten, Verkabelung bestimmt die Reihenfolge") {
    GridShape g;
    g.nx = 2;
    g.ny = 2;
    g.nz = 2;
    g.pitch = Vec3{2.0F, 4.0F, 6.0F};
    const FixtureShape shape = g;

    CHECK(ledCount(shape) == 8U);
    CHECK(localBox(shape) == Box{Vec3{0, 0, 0}, Vec3{4, 8, 12}});
    const auto positions = localPositions(shape);
    REQUIRE(positions.size() == 8U);
    checkNear(positions[0], 1.0F, 2.0F, 3.0F);
    checkNear(positions[1], 3.0F, 2.0F, 3.0F);  // x läuft am schnellsten
    checkNear(positions[2], 1.0F, 6.0F, 3.0F);
    checkNear(positions[4], 1.0F, 2.0F, 9.0F);
}

TEST_CASE("Punktwolke: Positionen unverändert, Box eng") {
    PointsShape p;
    p.points = {Vec3{2, -1, 5}, Vec3{-3, 4, 0}, Vec3{1, 1, 1}};
    const FixtureShape shape = p;
    CHECK(ledCount(shape) == 3U);
    CHECK(localBox(shape) == Box{Vec3{-3, -1, 0}, Vec3{2, 4, 5}});
    CHECK(localPositions(shape) == p.points);
}
