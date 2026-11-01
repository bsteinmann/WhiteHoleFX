// wiring.cpp: Umsetzung zu wiring.hpp. In welcher Reihenfolge hängen die LEDs am Datenkabel?
#include "whitehole/mapping/wiring.hpp"

#include <array>

namespace whitehole::mapping {

Cell2 matrixCell(std::uint64_t index, std::uint32_t cols, std::uint32_t rows,
                 const MatrixWiring& wiring) noexcept {
    const bool rowsFirst = wiring.first == FirstAxis::Rows;
    const std::uint64_t lineLength = rowsFirst ? cols : rows;

    const std::uint64_t line = index / lineLength;
    std::uint64_t pos = index % lineLength;
    if (wiring.serpentine && (line % 2U) == 1U) {
        pos = lineLength - 1U - pos;
    }

    auto column = static_cast<std::uint32_t>(rowsFirst ? pos : line);
    auto row = static_cast<std::uint32_t>(rowsFirst ? line : pos);

    if (wiring.start == StartCorner::TopRight || wiring.start == StartCorner::BottomRight) {
        column = cols - 1U - column;
    }
    if (wiring.start == StartCorner::BottomLeft || wiring.start == StartCorner::BottomRight) {
        row = rows - 1U - row;
    }
    return Cell2{column, row};
}

namespace {

/// Achsen in Laufreihenfolge (schnellste zuerst) als Indizes 0=x, 1=y, 2=z.
std::array<int, 3> axesOf(AxisOrder order) noexcept {
    switch (order) {
        case AxisOrder::XYZ: return {0, 1, 2};
        case AxisOrder::XZY: return {0, 2, 1};
        case AxisOrder::YXZ: return {1, 0, 2};
        case AxisOrder::YZX: return {1, 2, 0};
        case AxisOrder::ZXY: return {2, 0, 1};
        case AxisOrder::ZYX: return {2, 1, 0};
    }
    return {0, 1, 2};
}

}  // namespace

Cell3 gridCell(std::uint64_t index, std::uint32_t nx, std::uint32_t ny, std::uint32_t nz,
               const GridWiring& wiring) noexcept {
    const std::array<std::uint32_t, 3> size{nx, ny, nz};
    const std::array<int, 3> axes = axesOf(wiring.order);
    const std::uint64_t n0 = size[static_cast<std::size_t>(axes[0])];
    const std::uint64_t n1 = size[static_cast<std::size_t>(axes[1])];

    const std::uint64_t line = index / n0;   // laufende Nummer der Linie entlang Achse 0
    const std::uint64_t plane = line / n1;   // laufende Nummer der Ebene entlang Achse 1
    std::uint64_t c0 = index % n0;
    std::uint64_t c1 = line % n1;
    const std::uint64_t c2 = plane;

    if (wiring.serpentine) {
        // Achse 1 kehrt in jeder zweiten Ebene um, Achse 0 in jeder zweiten Linie.
        // So beginnt jede Linie dort, wo die vorige endete.
        if ((plane % 2U) == 1U) {
            c1 = n1 - 1U - c1;
        }
        if ((line % 2U) == 1U) {
            c0 = n0 - 1U - c0;
        }
    }

    std::array<std::uint32_t, 3> cell{};
    cell[static_cast<std::size_t>(axes[0])] = static_cast<std::uint32_t>(c0);
    cell[static_cast<std::size_t>(axes[1])] = static_cast<std::uint32_t>(c1);
    cell[static_cast<std::size_t>(axes[2])] = static_cast<std::uint32_t>(c2);
    // Startecke: die Zählung einer Achse von der anderen Seite her (Serpentine bleibt durchgehend, sie wird mitgespiegelt).
    const std::array<bool, 3> flip{wiring.flipX, wiring.flipY, wiring.flipZ};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (flip[axis]) {
            cell[axis] = size[axis] - 1U - cell[axis];
        }
    }
    return Cell3{cell[0], cell[1], cell[2]};
}

}  // namespace whitehole::mapping
