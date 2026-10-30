// Layout: Verwaltung der Fixtures und das Backen zur LedMap.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <utility>

#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using Catch::Approx;

namespace {

MatrixShape matrix(std::uint32_t cols, std::uint32_t rows) {
    MatrixShape m;
    m.columns = cols;
    m.rows = rows;
    return m;
}

/// Fügt ein Fixture ein, das gültig sein muss.
FixtureId put(Layout& layout, const char* name, const Vec3& origin, FixtureShape shape) {
    const auto id = layout.add(name, origin, std::move(shape));
    REQUIRE(id.ok());
    return id.value();
}

void checkNear(const Vec3& v, float x, float y, float z) {
    CHECK(v.x == Approx(x).margin(1e-5));
    CHECK(v.y == Approx(y).margin(1e-5));
    CHECK(v.z == Approx(z).margin(1e-5));
}

}  // namespace

TEST_CASE("Fixtures hinzufügen, finden, verschieben, ersetzen, entfernen") {
    Layout layout;
    const auto a = layout.add("Wand", Vec3{}, matrix(4, 2));
    const auto b = layout.add("Decke", Vec3{}, StripShape{});
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value().valid());
    CHECK(a.value() != b.value());
    REQUIRE(layout.fixtures().size() == 2U);
    CHECK(layout.find(a.value())->name == "Wand");

    REQUIRE(layout.move(a.value(), Vec3{5, 6, 7}).ok());
    CHECK(layout.find(a.value())->origin == Vec3{5, 6, 7});

    REQUIRE(layout.replaceShape(a.value(), matrix(8, 8)).ok());
    CHECK(ledCount(layout.find(a.value())->shape) == 64U);

    REQUIRE(layout.remove(a.value()).ok());
    CHECK(layout.find(a.value()) == nullptr);
    CHECK(layout.fixtures().size() == 1U);

    // IDs werden nicht wiederverwendet.
    const auto c = layout.add("Neu", Vec3{}, matrix(1, 1));
    REQUIRE(c.ok());
    CHECK(c.value() != a.value());
    CHECK(c.value() != b.value());
}

TEST_CASE("Ungültige Eingaben ändern das Layout nicht") {
    Layout layout;
    const FixtureId id = layout.add("Wand", Vec3{1, 2, 3}, matrix(4, 2)).value();

    CHECK_FALSE(layout.add("Leer", Vec3{}, matrix(0, 2)).ok());
    CHECK_FALSE(layout.add("NaN", Vec3{std::nanf(""), 0, 0}, matrix(1, 1)).ok());
    CHECK(layout.fixtures().size() == 1U);

    CHECK_FALSE(layout.replaceShape(id, matrix(0, 0)).ok());
    CHECK(ledCount(layout.find(id)->shape) == 8U);  // alte Form bleibt
    CHECK_FALSE(layout.move(id, Vec3{std::numeric_limits<float>::infinity(), 0, 0}).ok());
    CHECK(layout.find(id)->origin == Vec3{1, 2, 3});

    const FixtureId unknown{999};
    CHECK(layout.remove(unknown).error().code == ErrorCode::NotFound);
    CHECK(layout.move(unknown, Vec3{}).error().code == ErrorCode::NotFound);
    CHECK(layout.replaceShape(unknown, matrix(1, 1)).error().code == ErrorCode::NotFound);
}

TEST_CASE("Ein leeres Layout lässt sich nicht backen") {
    const Layout layout;
    const auto baked = layout.bake();
    REQUIRE_FALSE(baked.ok());
    CHECK(baked.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("Eine Matrix trifft dieselben Punkte wie die Pixel des Canvas") {
    // Canvas-Pixel (x, y) tastet p = ((x + 0.5) / w, 1 - (y + 0.5) / h, 0) ab (y nach oben).
    constexpr std::uint32_t kCols = 16;
    constexpr std::uint32_t kRows = 8;
    Layout layout;
    put(layout, "Panel", Vec3{}, matrix(kCols, kRows));
    const LedMap map = layout.bake().value();

    REQUIRE(map.size() == std::size_t{kCols} * kRows);
    for (std::uint32_t row = 0; row < kRows; ++row) {
        for (std::uint32_t col = 0; col < kCols; ++col) {
            const Vec3& p = map.unit[std::size_t{row} * kCols + col];
            CHECK(p.x == Approx((static_cast<float>(col) + 0.5F) / kCols).margin(1e-5));
            CHECK(p.y == Approx(1.0F - (static_cast<float>(row) + 0.5F) / kRows).margin(1e-5));
            CHECK(p.z == 0.0F);
        }
    }
}

TEST_CASE("Mehrere Fixtures teilen sich einen Raum und behalten ihre Bereiche") {
    Layout layout;
    const FixtureId left = put(layout, "Links", Vec3{0, 0, 0}, matrix(16, 8));
    const FixtureId right = put(layout, "Rechts", Vec3{16, 0, 0}, matrix(16, 8));
    const LedMap map = layout.bake().value();

    CHECK(map.volume == Box{Vec3{0, 0, 0}, Vec3{32, 8, 0}});
    REQUIRE(map.ranges.size() == 2U);
    CHECK(map.ranges[0] == FixtureRange{left, 0, 128});
    CHECK(map.ranges[1] == FixtureRange{right, 128, 128});

    checkNear(map.world[0], 0.5F, 7.5F, 0.0F);
    checkNear(map.world[128], 16.5F, 7.5F, 0.0F);
    // Das rechte Panel belegt die rechte Hälfte des Einheitswürfels.
    checkNear(map.unit[0], 0.5F / 32.0F, 1.0F - 0.5F / 8.0F, 0.0F);
    checkNear(map.unit[128], 16.5F / 32.0F, 1.0F - 0.5F / 8.0F, 0.0F);
}

TEST_CASE("Der Ursprung eines Fixtures verschiebt seine LEDs") {
    Layout layout;
    put(layout, "A", Vec3{}, matrix(2, 2));
    put(layout, "B", Vec3{10, 20, 30}, matrix(2, 2));
    const LedMap map = layout.bake().value();
    checkNear(map.world[4], 10.5F, 21.5F, 30.0F);
    CHECK(map.volume.max.z == Approx(30.0F));
}

TEST_CASE("KeepAspect behält die Proportionen und zentriert") {
    Layout layout;
    put(layout, "Wand", Vec3{}, matrix(32, 8));

    const LedMap stretched = layout.bake(NormalizeMode::Stretch).value();
    const LedMap aspect = layout.bake(NormalizeMode::KeepAspect).value();

    // Obere linke LED (Zeile 0, Spalte 0).
    checkNear(stretched.unit[0], 0.5F / 32.0F, 1.0F - 0.5F / 8.0F, 0.0F);
    // Gleicher Maßstab in x und y: 8 Einheiten Höhe belegen ein Viertel der Breite,
    // zentriert um 0,5.
    checkNear(aspect.unit[0], 0.5F / 32.0F, 0.5F + 3.5F / 32.0F, 0.0F);
    // Untere Zeile liegt entsprechend darunter.
    checkNear(aspect.unit[7 * 32], 0.5F / 32.0F, 0.5F - 3.5F / 32.0F, 0.0F);
}

TEST_CASE("Ein Würfel füllt den Einheitswürfel in beiden Modi gleich") {
    GridShape g;
    g.nx = g.ny = g.nz = 4;
    Layout layout;
    put(layout, "Cube", Vec3{}, g);
    for (const NormalizeMode mode : {NormalizeMode::Stretch, NormalizeMode::KeepAspect}) {
        const LedMap map = layout.bake(mode).value();
        REQUIRE(map.size() == 64U);
        checkNear(map.unit.front(), 0.125F, 0.125F, 0.125F);
        checkNear(map.unit.back(), 0.875F, 0.875F, 0.875F);
    }
}

TEST_CASE("Achsen ohne Ausdehnung bleiben bei 0") {
    Layout layout;
    StripShape s;
    s.count = 10;
    s.start = Vec3{0, 5, 5};
    s.end = Vec3{10, 5, 5};
    put(layout, "Strip", Vec3{}, s);
    for (const NormalizeMode mode : {NormalizeMode::Stretch, NormalizeMode::KeepAspect}) {
        const LedMap map = layout.bake(mode).value();
        for (const Vec3& p : map.unit) {
            CHECK(p.y == 0.0F);
            CHECK(p.z == 0.0F);
        }
        CHECK(map.unit.front().x == Approx(0.05F).margin(1e-5));
        CHECK(map.unit.back().x == Approx(0.95F).margin(1e-5));
    }

    // Eine einzelne LED ergibt überall 0 und keine Division durch 0.
    Layout single;
    put(single, "Punkt", Vec3{3, 4, 5}, matrix(1, 1));
    const LedMap map = single.bake().value();
    checkNear(map.unit[0], 0.5F, 0.5F, 0.0F);
}

TEST_CASE("Alle Einheitskoordinaten liegen in [0,1]") {
    Layout layout;
    put(layout, "A", Vec3{-7, 3, 2}, matrix(5, 3));
    GridShape g;
    g.nx = 3;
    g.ny = 4;
    g.nz = 5;
    g.pitch = Vec3{0.3F, 1.7F, 2.9F};
    put(layout, "B", Vec3{4, -2, -9}, g);
    PointsShape p;
    p.points = {Vec3{100, 100, 100}, Vec3{-50, 0, 7}};
    put(layout, "C", Vec3{}, p);

    for (const NormalizeMode mode : {NormalizeMode::Stretch, NormalizeMode::KeepAspect}) {
        const LedMap map = layout.bake(mode).value();
        REQUIRE(map.unit.size() == map.world.size());
        for (const Vec3& u : map.unit) {
            CHECK(u.x >= 0.0F);
            CHECK(u.x <= 1.0F);
            CHECK(u.y >= 0.0F);
            CHECK(u.y <= 1.0F);
            CHECK(u.z >= 0.0F);
            CHECK(u.z <= 1.0F);
        }
    }
}

TEST_CASE("Zu viele LEDs werden abgelehnt, bevor Speicher belegt wird") {
    Layout layout;
    for (int i = 0; i < 3; ++i) {
        REQUIRE(layout.add("Riesig", Vec3{}, matrix(2048, 2048)).ok());  // 3 x 4,2 Mio.
    }
    const auto baked = layout.bake();
    REQUIRE_FALSE(baked.ok());
    CHECK(baked.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("Rundungsfehler an den Rändern der Box schieben keine LED aus [0,1]") {
    // Die Randpunkte der Box liegen exakt auf 0 und 1; ohne Begrenzung könnte die
    // Float-Rechnung sie um ein Ulp darüber hinaus tragen.
    const float lows[] = {-7.3F, 0.1F, 3.33F, 1000.7F, -123456.7F};
    const float extents[] = {0.3F, 1.7F, 123.456F, 0.001F, 99999.9F};
    for (const float low : lows) {
        for (const float extent : extents) {
            CAPTURE(low, extent);
            PointsShape points;
            points.points = {Vec3{low, low, low}, Vec3{low + extent, low + extent, low + extent},
                             Vec3{low + extent * 0.37F, low + extent * 0.91F, low + extent * 0.5F}};
            Layout layout;
            put(layout, "Wolke", Vec3{}, points);
            for (const NormalizeMode mode : {NormalizeMode::Stretch, NormalizeMode::KeepAspect}) {
                const LedMap map = layout.bake(mode).value();
                for (const Vec3& u : map.unit) {
                    CHECK(u.x >= 0.0F);
                    CHECK(u.x <= 1.0F);
                    CHECK(u.y >= 0.0F);
                    CHECK(u.y <= 1.0F);
                    CHECK(u.z >= 0.0F);
                    CHECK(u.z <= 1.0F);
                }
            }
        }
    }
}
