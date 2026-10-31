// fixture.hpp: ein physisches Leuchtobjekt (Matrix, Strip, 3D-Gitter, Punktwolke).
//
// Warum es diese Datei gibt: Die Software soll LEDs nicht nur auf einer Fläche, sondern
// im Raum ansteuern. Ein Fixture beschreibt eines dieser Objekte so, dass daraus zwei
// Dinge folgen: die Position jeder LED und ihre Reihenfolge am Datenkabel. Mehrere
// Fixtures bilden zusammen ein Layout (layout.hpp), z. B. eine Wand aus vier Panels
// plus ein Strip an der Decke.
//
// Aufgabe: Datentypen für die vier Formen, Prüfung der Eingaben und die Berechnung der
// lokalen Positionen. Alles reine Rechenlogik ohne GPU.
//
// Koordinaten (gelten im ganzen Projekt):
//  - Rechtshändig: x nach rechts, y nach oben, z zum Betrachter.
//  - Längen sind frei wählbare Einheiten (z. B. Zentimeter). Erst das Layout bildet sie
//    auf den Einheitswürfel [0,1]^3 ab, den die Effekte sehen.
//  - Jede LED sitzt in der Mitte ihrer Zelle. Eine Matrix mit Raster 1 und 16 Spalten
//    belegt also x in [0,16], die erste LED sitzt bei x = 0,5. So tastet das Mapping
//    exakt dieselben Punkte ab wie der Canvas (Pixelmitten).
//  - Matrix: Zeile 0 ist oben (wie in einem Bild). Gitter: y = 0 ist unten (wie im Raum).
//
// Nicht enthalten (späteres Release): Drehung von Fixtures, Teilstücke mit Lücken,
// gekrümmte Strips. Punktwolken decken diese Fälle bis dahin ab.
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/mapping/wiring.hpp"

namespace whitehole::mapping {

struct FixtureTag {};
using FixtureId = StrongId<FixtureTag>;

/// Obergrenzen, damit fehlerhafte Eingaben keinen Speicher sprengen.
inline constexpr std::uint64_t kMaxLedsPerFixture = 1U << 22;  // rund 4 Mio.

struct MatrixShape {
    std::uint32_t columns{1};
    std::uint32_t rows{1};
    Vec2 pitch{1.0F, 1.0F};  // Abstand der Zellmitten in x und y
    MatrixWiring wiring;
};

struct StripShape {
    std::uint32_t count{1};
    Vec3 start;  // relativ zum Ursprung des Fixtures
    Vec3 end;    // die LEDs sitzen in der Mitte von `count` gleich langen Abschnitten
};

struct GridShape {
    std::uint32_t nx{1};
    std::uint32_t ny{1};
    std::uint32_t nz{1};
    Vec3 pitch{1.0F, 1.0F, 1.0F};
    GridWiring wiring;
};

struct PointsShape {
    std::vector<Vec3> points;  // in Kabelreihenfolge, relativ zum Ursprung
};

using FixtureShape = std::variant<MatrixShape, StripShape, GridShape, PointsShape>;

struct Fixture {
    FixtureId id;
    std::string name;
    Vec3 origin;  // Lage des Objekts im Raum
    FixtureShape shape;
};

/// Achsenparallele Box.
struct Box {
    Vec3 min;
    Vec3 max;
    bool operator==(const Box&) const = default;
};

/// Prüft Maße, Abstände und Zahlen (endlich, positiv, unter der Obergrenze).
/// Fehler: InvalidArgument.
[[nodiscard]] Result<void> validateShape(const FixtureShape& shape);

/// Anzahl der LEDs. Voraussetzung: validateShape war erfolgreich.
[[nodiscard]] std::uint64_t ledCount(const FixtureShape& shape) noexcept;

/// Umschließende Box der Zellen in lokalen Koordinaten (Ursprung 0).
[[nodiscard]] Box localBox(const FixtureShape& shape) noexcept;

/// Abstand zwischen einer LED und ihrer nächsten Nachbarin, in Welteinheiten (kleinster Abstand
/// der Form; bei Matrix und Gitter der kleinere Abstand der Achsen mit mehr als einer LED). Die
/// Vorschau macht die Punkte so groß, dass sie sich nicht überlappen. 0 = unbekannt (eine einzelne
/// LED). Punktwolken: aus einer Stichprobe, ab 20000 Punkten 0.
[[nodiscard]] float ledSpacing(const FixtureShape& shape) noexcept;

/// Lokale Position jeder LED in Kabelreihenfolge (Index 0 = erste LED am Kabel).
[[nodiscard]] std::vector<Vec3> localPositions(const FixtureShape& shape);

}  // namespace whitehole::mapping
