// wiring.hpp: in welcher Reihenfolge hängen die LEDs am Datenkabel?
//
// Warum es diese Datei gibt: Ein Effekt kennt nur Positionen. Die Hardware kennt nur
// "LED Nummer i am Kabel". Dazwischen steht die Verkabelung: Wo beginnt der Strang
// (Ecke), läuft er zeilen- oder spaltenweise, kehrt er an jedem Ende um (Serpentine)?
// Diese Datei beantwortet für Index i die Frage "welche Zelle ist das?". Sie ist reine
// Arithmetik und deshalb vollständig testbar (jede Zelle genau einmal, Nachbarn bei
// Serpentine wirklich benachbart).
//
// Abhängigkeiten: keine. Spätere Schritte (Ausgabe) nutzen dieselbe Reihenfolge, um
// Pixel in Universen zu packen.
#pragma once

#include <cstdint>

namespace whitehole::mapping {

/// Ecke, an der der Strang beginnt. Zeile 0 ist oben, Spalte 0 links.
enum class StartCorner : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

/// Richtung, in der der Strang zuerst läuft.
enum class FirstAxis : std::uint8_t { Rows, Columns };

struct MatrixWiring {
    StartCorner start{StartCorner::TopLeft};
    FirstAxis first{FirstAxis::Rows};
    bool serpentine{false};  // jede zweite Zeile (Spalte) läuft rückwärts

    bool operator==(const MatrixWiring&) const = default;
};

struct Cell2 {
    std::uint32_t column{0};
    std::uint32_t row{0};
    bool operator==(const Cell2&) const = default;
};

/// Zelle der LED Nummer `index` (0-basiert) in einer cols x rows Matrix.
/// Voraussetzung: cols, rows > 0 und index < cols * rows.
[[nodiscard]] Cell2 matrixCell(std::uint64_t index, std::uint32_t cols, std::uint32_t rows,
                               const MatrixWiring& wiring) noexcept;

/// Achsenfolge eines 3D-Gitters: Die erste Achse läuft am schnellsten.
enum class AxisOrder : std::uint8_t { XYZ, XZY, YXZ, YZX, ZXY, ZYX };

struct GridWiring {
    AxisOrder order{AxisOrder::XYZ};
    bool serpentine{false};  // Umkehr an jedem Ende, in allen Ebenen (durchgehender Strang)
    /// Spiegelt die Zählung je Achse: Der Strang beginnt dann am oberen statt am unteren Ende dieser Achse
    /// (Startecke des Kabels; (0,0,0) ist unten links hinten, x nach rechts, y nach oben, z zum Betrachter).
    bool flipX{false};
    bool flipY{false};
    bool flipZ{false};

    bool operator==(const GridWiring&) const = default;
};

struct Cell3 {
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t z{0};
    bool operator==(const Cell3&) const = default;
};

/// Zelle der LED Nummer `index` in einem nx x ny x nz Gitter.
/// Voraussetzung: alle Maße > 0 und index < nx * ny * nz.
[[nodiscard]] Cell3 gridCell(std::uint64_t index, std::uint32_t nx, std::uint32_t ny,
                             std::uint32_t nz, const GridWiring& wiring) noexcept;

}  // namespace whitehole::mapping
