// Projektion je Fixture (Faces, Box): hohler Würfel aus sechs Matrizen als gefaltete 2D-Fläche, Mischanlagen, Sonderfälle.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>
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

FixtureId put(Layout& layout, const char* name, const Vec3& origin, FixtureShape shape, const Vec3& rotation,
              Projection projection) {
    const auto id = layout.add(name, origin, std::move(shape));
    REQUIRE(id.ok());
    REQUIRE(layout.rotate(id.value(), rotation).ok());
    REQUIRE(layout.setProjection(id.value(), projection).ok());
    return id.value();
}

/// Hohler Würfel wie examples/3d/wuerfel-hohl.whfx: sechs Matrizen n x n, Kante n, nur die Außenhaut.
Layout cube(std::uint32_t n, Projection projection) {
    Layout layout;
    const auto h = static_cast<float>(n) * 0.5F;
    const auto e = static_cast<float>(n);
    put(layout, "Vorne", Vec3{0, 0, 0}, matrix(n, n), Vec3{0, 0, 0}, projection);
    put(layout, "Hinten", Vec3{0, 0, e}, matrix(n, n), Vec3{0, 180, 0}, projection);
    put(layout, "Links", Vec3{-h, 0, h}, matrix(n, n), Vec3{0, 90, 0}, projection);
    put(layout, "Rechts", Vec3{h, 0, h}, matrix(n, n), Vec3{0, -90, 0}, projection);
    put(layout, "Oben", Vec3{0, h, h}, matrix(n, n), Vec3{90, 0, 0}, projection);
    put(layout, "Unten", Vec3{0, -h, h}, matrix(n, n), Vec3{-90, 0, 0}, projection);
    return layout;
}

/// Zellindex (0..n-1) einer Koordinate in [0,1], wenn die LEDs in der Mitte von n Zellen sitzen.
int cellOf(float v, int n) { return static_cast<int>(std::lround(v * static_cast<float>(n) - 0.5F)); }

}  // namespace

TEST_CASE("Projektion: Volume bleibt die Raumposition, der hohle Würfel hat dann Tiefe", "[mapping][projection]") {
    const Layout layout = cube(8, Projection::Volume);
    const auto map = layout.bake();
    REQUIRE(map.ok());
    bool anyDepth = false;
    for (const Vec3& p : map.value().unit) {
        anyDepth = anyDepth || p.z > 0.0F;
    }
    CHECK(anyDepth);
}

TEST_CASE("Projektion: Box legt jede Würfelfläche als volles 2D-Bild auf 0..1", "[mapping][projection][box]") {
    constexpr int n = 8;
    const Layout layout = cube(n, Projection::Box);
    const auto map = layout.bake();
    REQUIRE(map.ok());
    REQUIRE(map.value().unit.size() == 6U * n * n);
    for (const Vec3& p : map.value().unit) {
        CHECK(p.z == 0.0F);
    }
    for (const FixtureRange& range : map.value().ranges) {
        std::set<std::pair<int, int>> cells;
        for (std::uint32_t k = 0; k < range.count; ++k) {
            const Vec3& p = map.value().unit[range.first + k];
            cells.emplace(cellOf(p.x, n), cellOf(p.y, n));
        }
        CHECK(cells.size() == static_cast<std::size_t>(n) * n);  // jede Zelle genau einmal
        CHECK(cells.begin()->first == 0);
        CHECK(cells.rbegin()->first == n - 1);
    }
}

TEST_CASE("Projektion: Box richtet die Flächen nach der Lage (vorne x/y, seitlich z/y, oben x/z)", "[mapping][projection][box]") {
    constexpr int n = 8;
    const Layout layout = cube(n, Projection::Box);
    const auto map = layout.bake();
    REQUIRE(map.ok());
    const auto rangeOf = [&](const char* name) {
        for (const Fixture& f : layout.fixtures()) {
            if (f.name == name) {
                for (const FixtureRange& r : map.value().ranges) {
                    if (r.fixture == f.id) {
                        return r;
                    }
                }
            }
        }
        FAIL("Fixture nicht gefunden: " << name);
        return FixtureRange{};
    };
    // Die Weltposition der Raumkoordinate bestimmt (u, v): vorne (x, y), links (z, y), oben (x, z), normiert auf die Box.
    const auto check = [&](const char* name, int uAxis, int vAxis) {
        const FixtureRange r = rangeOf(name);
        for (std::uint32_t k = 0; k < r.count; ++k) {
            const Vec3& w = map.value().world[r.first + k];
            const Vec3& p = map.value().unit[r.first + k];
            const auto at = [&](int axis) { return axis == 0 ? w.x : axis == 1 ? w.y : w.z; };
            INFO(name << " LED " << k);
            CHECK(p.x == Approx(at(uAxis) / static_cast<float>(n)).margin(1e-4));
            CHECK(p.y == Approx(at(vAxis) / static_cast<float>(n)).margin(1e-4));
        }
    };
    check("Vorne", 0, 1);
    check("Hinten", 0, 1);
    check("Links", 2, 1);
    check("Rechts", 2, 1);
    check("Oben", 0, 2);
    check("Unten", 0, 2);
}

TEST_CASE("Projektion: Faces gibt jedem Fixture sein eigenes volles Bild, auch gedreht", "[mapping][projection][faces]") {
    constexpr int n = 8;
    const Layout layout = cube(n, Projection::Faces);
    const auto map = layout.bake();
    REQUIRE(map.ok());
    for (const FixtureRange& range : map.value().ranges) {
        std::set<std::pair<int, int>> cells;
        for (std::uint32_t k = 0; k < range.count; ++k) {
            const Vec3& p = map.value().unit[range.first + k];
            CHECK(p.z == 0.0F);
            cells.emplace(cellOf(p.x, n), cellOf(p.y, n));
        }
        CHECK(cells.size() == static_cast<std::size_t>(n) * n);
    }
    // Die ersten LEDs der Flächen gleichen sich: gleiche Matrix, gleiches Bild, egal wie sie im Raum liegen.
    const auto& u = map.value().unit;
    CHECK(u[0] == u[static_cast<std::size_t>(n) * n]);
}

TEST_CASE("Projektion: KeepAspect behält das Seitenverhältnis einer breiten Matrix (Faces)", "[mapping][projection][faces]") {
    Layout layout;
    put(layout, "Wand", Vec3{0, 0, 0}, matrix(32, 16), Vec3{}, Projection::Faces);
    const auto stretch = layout.bake(NormalizeMode::Stretch);
    const auto keep = layout.bake(NormalizeMode::KeepAspect);
    REQUIRE(stretch.ok());
    REQUIRE(keep.ok());
    // Stretch füllt 0..1 in beiden Achsen, KeepAspect nur die Hälfte in y (zentriert).
    float maxYStretch = 0.0F;
    float maxYKeep = 0.0F;
    for (const Vec3& p : stretch.value().unit) {
        maxYStretch = std::max(maxYStretch, p.y);
    }
    for (const Vec3& p : keep.value().unit) {
        maxYKeep = std::max(maxYKeep, p.y);
    }
    CHECK(maxYStretch > 0.95F);
    CHECK(maxYKeep < 0.8F);
}

TEST_CASE("Projektion: eine Mischanlage normalisiert Volume-Fixtures nur auf ihre eigene Box", "[mapping][projection][mixed]") {
    Layout layout;
    // Ein Würfel weit weg, daneben eine freistehende Matrix mit der alten Raumabbildung.
    const Layout source = cube(4, Projection::Box);
    for (const Fixture& f : source.fixtures()) {
        const auto id = layout.add(f.name, Vec3{f.origin.x + 1000.0F, f.origin.y, f.origin.z}, f.shape);
        REQUIRE(id.ok());
        REQUIRE(layout.rotate(id.value(), f.rotation).ok());
        REQUIRE(layout.setProjection(id.value(), Projection::Box).ok());
    }
    const FixtureId solo = put(layout, "Solo", Vec3{0, 0, 0}, matrix(8, 8), Vec3{}, Projection::Volume);
    const auto map = layout.bake();
    REQUIRE(map.ok());
    for (const FixtureRange& r : map.value().ranges) {
        if (r.fixture != solo) {
            continue;
        }
        float maxX = 0.0F;
        for (std::uint32_t k = 0; k < r.count; ++k) {
            maxX = std::max(maxX, map.value().unit[r.first + k].x);
        }
        CHECK(maxX > 0.9F);  // volle Breite, nicht auf die 1000 Einheiten der Anlage gestaucht
    }
}

TEST_CASE("Projektion: eine flache Wand mit Box ist einfach (x, y)", "[mapping][projection][box]") {
    Layout layout;
    put(layout, "Wand", Vec3{0, 0, 0}, matrix(8, 4), Vec3{}, Projection::Box);
    const auto map = layout.bake();
    REQUIRE(map.ok());
    for (const Vec3& p : map.value().unit) {
        CHECK(p.z == 0.0F);
    }
    CHECK(map.value().unit.front().x == Approx(0.5 / 8).margin(1e-4));
}

TEST_CASE("Projektion: unbekanntes Fixture beim Umstellen ist ein Fehler", "[mapping][projection]") {
    Layout layout;
    const auto r = layout.setProjection(FixtureId{99}, Projection::Box);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == ErrorCode::NotFound);
}
