// fixture_snap.hpp: Einrasten an den Kanten und Mitten anderer Fixtures, mit Hilfslinien.
//
// Warum es diese Datei gibt: Das Raster hilft, wenn Fixtures auf ganzen Einheiten liegen. Fixtures, die genau
// bündig oder mittig zu einem Nachbarn stehen sollen (zwei Matrizen nebeneinander, ein Strip über der Mitte einer
// Matrix), treffen kein Raster. Wie in einem Grafikprogramm rasten hier die Kanten und die Mitte der gezogenen
// Fixtures an Kanten und Mitten der übrigen ein, sobald sie nahe genug sind; die Hilfslinien zeigen, woran.
// Reine Rechnung ohne Qt (tests/preview/fixture_snap_test.cpp). Die Oberfläche (PreviewController) ruft sie beim
// Ziehen auf und zeichnet die Linien.
#pragma once

#include <span>
#include <vector>

namespace whitehole::preview {

/// Achsenparalleles Rechteck in Welteinheiten (x nach rechts, y nach oben).
struct SnapRect {
    float minX{0.0F};
    float minY{0.0F};
    float maxX{0.0F};
    float maxY{0.0F};
};

/// Eine Hilfslinie: senkrecht bei x = position (vertical) oder waagrecht bei y = position, von `from` bis `to`
/// entlang der Linie (in Welteinheiten).
struct SnapGuide {
    bool vertical{false};
    float position{0.0F};
    float from{0.0F};
    float to{0.0F};
};

struct FixtureSnap {
    float dx{0.0F};  // Verschiebung, um die das bewegte Rechteck einrastet (0, wenn in der Richtung nichts nah genug ist)
    float dy{0.0F};
    bool snappedX{false};
    bool snappedY{false};
    std::vector<SnapGuide> guides;  // Linien am eingerasteten Ort (alle Treffer, nicht nur der gewählte)
};

/// Rastet `moving` an `others` ein: Kante links/Mitte/Kante rechts gegen die gleichen Linien der anderen
/// (und ebenso in y), höchstens `threshold` Welteinheiten weit. Pro Richtung gewinnt der kleinste Abstand.
[[nodiscard]] FixtureSnap snapToFixtures(const SnapRect& moving, std::span<const SnapRect> others, float threshold);

}  // namespace whitehole::preview
