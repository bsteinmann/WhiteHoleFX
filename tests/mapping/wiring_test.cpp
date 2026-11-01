// Verkabelungsreihenfolge: Jede Zelle genau einmal, richtiger Start, Serpentine hängt durch.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <set>
#include <tuple>
#include <vector>

#include "whitehole/mapping/wiring.hpp"

using namespace whitehole::mapping;

namespace {

int dist(const Cell2& a, const Cell2& b) {
    return std::abs(static_cast<int>(a.column) - static_cast<int>(b.column)) +
           std::abs(static_cast<int>(a.row) - static_cast<int>(b.row));
}
int dist(const Cell3& a, const Cell3& b) {
    return std::abs(static_cast<int>(a.x) - static_cast<int>(b.x)) +
           std::abs(static_cast<int>(a.y) - static_cast<int>(b.y)) +
           std::abs(static_cast<int>(a.z) - static_cast<int>(b.z));
}

constexpr StartCorner kCorners[] = {StartCorner::TopLeft, StartCorner::TopRight,
                                    StartCorner::BottomLeft, StartCorner::BottomRight};
constexpr AxisOrder kOrders[] = {AxisOrder::XYZ, AxisOrder::XZY, AxisOrder::YXZ,
                                 AxisOrder::YZX, AxisOrder::ZXY, AxisOrder::ZYX};

}  // namespace

TEST_CASE("Matrix: Zeile für Zeile von links oben") {
    const MatrixWiring wiring;
    CHECK(matrixCell(0, 4, 2, wiring) == Cell2{0, 0});
    CHECK(matrixCell(3, 4, 2, wiring) == Cell2{3, 0});
    CHECK(matrixCell(4, 4, 2, wiring) == Cell2{0, 1});
    CHECK(matrixCell(7, 4, 2, wiring) == Cell2{3, 1});
}

TEST_CASE("Matrix: Serpentine kehrt in jeder zweiten Zeile um") {
    MatrixWiring wiring;
    wiring.serpentine = true;
    CHECK(matrixCell(3, 4, 2, wiring) == Cell2{3, 0});
    CHECK(matrixCell(4, 4, 2, wiring) == Cell2{3, 1});
    CHECK(matrixCell(7, 4, 2, wiring) == Cell2{0, 1});
}

TEST_CASE("Matrix: spaltenweise") {
    MatrixWiring wiring;
    wiring.first = FirstAxis::Columns;
    CHECK(matrixCell(0, 4, 2, wiring) == Cell2{0, 0});
    CHECK(matrixCell(1, 4, 2, wiring) == Cell2{0, 1});
    CHECK(matrixCell(2, 4, 2, wiring) == Cell2{1, 0});
    CHECK(matrixCell(7, 4, 2, wiring) == Cell2{3, 1});
}

TEST_CASE("Matrix: Startecken") {
    MatrixWiring wiring;
    wiring.start = StartCorner::TopRight;
    CHECK(matrixCell(0, 4, 2, wiring) == Cell2{3, 0});
    wiring.start = StartCorner::BottomLeft;
    CHECK(matrixCell(0, 4, 2, wiring) == Cell2{0, 1});
    wiring.start = StartCorner::BottomRight;
    CHECK(matrixCell(0, 4, 2, wiring) == Cell2{3, 1});
    CHECK(matrixCell(4, 4, 2, wiring) == Cell2{3, 0});
}

TEST_CASE("Matrix: alle Varianten besuchen jede Zelle genau einmal") {
    const std::pair<std::uint32_t, std::uint32_t> sizes[] = {{5, 3}, {3, 5}, {1, 4}, {4, 1}, {1, 1}, {8, 8}};
    for (const auto& [cols, rows] : sizes) {
        for (const StartCorner corner : kCorners) {
            for (const FirstAxis first : {FirstAxis::Rows, FirstAxis::Columns}) {
                for (const bool serp : {false, true}) {
                    CAPTURE(cols, rows, static_cast<int>(corner), static_cast<int>(first), serp);
                    const MatrixWiring wiring{corner, first, serp};
                    std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
                    std::vector<Cell2> order;
                    for (std::uint64_t i = 0; i < std::uint64_t{cols} * rows; ++i) {
                        const Cell2 c = matrixCell(i, cols, rows, wiring);
                        REQUIRE(c.column < cols);
                        REQUIRE(c.row < rows);
                        seen.insert({c.column, c.row});
                        order.push_back(c);
                    }
                    CHECK(seen.size() == std::size_t{cols} * rows);

                    // Der Strang beginnt in der gewählten Ecke.
                    const bool right = corner == StartCorner::TopRight || corner == StartCorner::BottomRight;
                    const bool bottom = corner == StartCorner::BottomLeft || corner == StartCorner::BottomRight;
                    CHECK(order.front() == Cell2{right ? cols - 1U : 0U, bottom ? rows - 1U : 0U});

                    if (serp) {
                        for (std::size_t i = 1; i < order.size(); ++i) {
                            CHECK(dist(order[i - 1], order[i]) == 1);  // durchgehender Strang
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("Gitter: x läuft am schnellsten, dann y, dann z") {
    const GridWiring wiring;
    CHECK(gridCell(0, 3, 4, 2, wiring) == Cell3{0, 0, 0});
    CHECK(gridCell(1, 3, 4, 2, wiring) == Cell3{1, 0, 0});
    CHECK(gridCell(3, 3, 4, 2, wiring) == Cell3{0, 1, 0});
    CHECK(gridCell(12, 3, 4, 2, wiring) == Cell3{0, 0, 1});
    CHECK(gridCell(23, 3, 4, 2, wiring) == Cell3{2, 3, 1});
}

TEST_CASE("Gitter: andere Achsenfolgen") {
    GridWiring wiring;
    wiring.order = AxisOrder::ZYX;
    CHECK(gridCell(1, 3, 4, 2, wiring) == Cell3{0, 0, 1});
    CHECK(gridCell(2, 3, 4, 2, wiring) == Cell3{0, 1, 0});
    CHECK(gridCell(8, 3, 4, 2, wiring) == Cell3{1, 0, 0});
    wiring.order = AxisOrder::YXZ;
    CHECK(gridCell(1, 3, 4, 2, wiring) == Cell3{0, 1, 0});
    CHECK(gridCell(4, 3, 4, 2, wiring) == Cell3{1, 0, 0});
}

TEST_CASE("Gitter: alle Varianten besuchen jede Zelle genau einmal, Serpentine hängt durch") {
    const std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> sizes[] = {
        {3, 4, 2}, {2, 2, 2}, {4, 1, 3}, {1, 1, 1}, {5, 1, 1}, {1, 3, 1}, {3, 3, 3}};
    for (const auto& [nx, ny, nz] : sizes) {
        for (const AxisOrder order : kOrders) {
            for (const bool serp : {false, true}) {
                CAPTURE(nx, ny, nz, static_cast<int>(order), serp);
                const GridWiring wiring{order, serp};
                std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> seen;
                std::vector<Cell3> cells;
                for (std::uint64_t i = 0; i < std::uint64_t{nx} * ny * nz; ++i) {
                    const Cell3 c = gridCell(i, nx, ny, nz, wiring);
                    REQUIRE(c.x < nx);
                    REQUIRE(c.y < ny);
                    REQUIRE(c.z < nz);
                    seen.insert({c.x, c.y, c.z});
                    cells.push_back(c);
                }
                CHECK(seen.size() == std::size_t{nx} * ny * nz);
                CHECK(cells.front() == Cell3{0, 0, 0});
                if (serp) {
                    for (std::size_t i = 1; i < cells.size(); ++i) {
                        CHECK(dist(cells[i - 1], cells[i]) == 1);
                    }
                }
            }
        }
    }
}

TEST_CASE("Gitter: Startecke je Achse spiegeln (flip)") {
    GridWiring wiring;
    wiring.flipX = true;
    CHECK(gridCell(0, 3, 4, 2, wiring) == Cell3{2, 0, 0});
    CHECK(gridCell(1, 3, 4, 2, wiring) == Cell3{1, 0, 0});
    wiring.flipY = true;
    wiring.flipZ = true;
    CHECK(gridCell(0, 3, 4, 2, wiring) == Cell3{2, 3, 1});
    CHECK(gridCell(23, 3, 4, 2, wiring) == Cell3{0, 0, 0});
}

TEST_CASE("Gitter: mit jeder Startecke wird jede Zelle genau einmal besucht, Serpentine bleibt durchgehend") {
    const std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> sizes[] = {{3, 4, 2}, {2, 2, 2}, {4, 1, 3}, {1, 1, 1}};
    for (const auto& [nx, ny, nz] : sizes) {
        for (const AxisOrder order : kOrders) {
            for (int corner = 0; corner < 8; ++corner) {
                for (const bool serp : {false, true}) {
                    CAPTURE(nx, ny, nz, static_cast<int>(order), corner, serp);
                    GridWiring wiring{order, serp};
                    wiring.flipX = (corner & 1) != 0;
                    wiring.flipY = (corner & 2) != 0;
                    wiring.flipZ = (corner & 4) != 0;
                    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> seen;
                    std::vector<Cell3> cells;
                    for (std::uint64_t i = 0; i < std::uint64_t{nx} * ny * nz; ++i) {
                        const Cell3 c = gridCell(i, nx, ny, nz, wiring);
                        REQUIRE(c.x < nx);
                        REQUIRE(c.y < ny);
                        REQUIRE(c.z < nz);
                        seen.insert({c.x, c.y, c.z});
                        cells.push_back(c);
                    }
                    CHECK(seen.size() == std::size_t{nx} * ny * nz);
                    CHECK(cells.front() == Cell3{wiring.flipX ? nx - 1 : 0, wiring.flipY ? ny - 1 : 0, wiring.flipZ ? nz - 1 : 0});
                    if (serp) {
                        for (std::size_t i = 1; i < cells.size(); ++i) {
                            CHECK(dist(cells[i - 1], cells[i]) == 1);
                        }
                    }
                }
            }
        }
    }
}
