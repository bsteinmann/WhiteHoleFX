// grid.hpp: Raster für den Layout-Editor: Linien im Bild und Einrasten von Weltwerten.
//
// Warum es diese Datei gibt: Fixtures lassen sich im Layout-Editor schlecht von Hand ausrichten. Ein Raster
// zeigt die Maßeinheit, und beim Ziehen rastet die linke untere Ecke der gezogenen Fixtures darauf ein.
// Beides ist reine Rechnung (kein Qt), damit sie getestet wird (tests/preview/grid_test.cpp); die Oberfläche
// zeichnet nur die Linien.
//
// Ist das Raster bei kleinem Zoom zu dicht, wird die Weite automatisch vergröbert (2, 5, 10 ...), damit
// zwischen zwei Linien mindestens kMinGridPixels liegen.
#pragma once

#include <vector>

#include "whitehole/preview/view.hpp"

namespace whitehole::preview {

inline constexpr float kMinGridPixels = 8.0F;
inline constexpr int kMaxGridLines = 600;  // Obergrenze je Richtung, schützt vor winzigen Weiten

struct GridLine {
    float pixel{0.0F};  // x (senkrechte Linie) oder y (waagrechte Linie) im Bild
    bool major{false};  // jede zehnte Linie
};

struct Grid {
    float step{0.0F};  // tatsächlich benutzte Weite in Welteinheiten (nach dem Vergröbern)
    std::vector<GridLine> vertical;
    std::vector<GridLine> horizontal;
};

/// Rastert `value` auf das nächste Vielfache von `step`. Bei `step` <= 0 bleibt der Wert unverändert.
[[nodiscard]] float snapToGrid(float value, float step) noexcept;

/// Die Weite, die bei diesem Maßstab tatsächlich gezeichnet wird (mindestens kMinGridPixels Abstand).
[[nodiscard]] float effectiveGridStep(float step, float pixelsPerUnit) noexcept;

/// Linien für ein Bild der Größe width x height in der Ebene z = 0. Leer bei ungültiger Weite oder Größe.
[[nodiscard]] Grid buildGrid(const ViewFrame& frame, float width, float height, float step);

}  // namespace whitehole::preview
