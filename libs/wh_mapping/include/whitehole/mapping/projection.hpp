// projection.hpp: p für Fixtures, die als 2D-Fläche abgebildet werden (Faces, Box).
//
// Warum es diese Datei gibt: Ein hohler Würfel aus sechs Matrizen ist im Raum eine 3D-Form, soll aber wie eine gefaltete
// 2D-Fläche leuchten. Effekte sehen nur p; wer p setzt, entscheidet also, was ein flächenbasierter Effekt (Labyrinth,
// Text, Pong, ...) zu sehen bekommt. Das gehört in das Backen der LedMap und nicht in die einzelnen Nodes.
// Entwurf und Entscheidungen: docs/entwurf-flaechenabbildung.md.
//
//  Faces: unit.xy = Lage der LED in der eigenen (ungedrehten) Box des Fixtures, unit.z = 0.
//  Box:   Das Fixture wird von außen auf die Box aller Box-Fixtures projiziert (unit.z = 0). Die Fläche richtet sich
//         bei ebenen Fixtures (Matrix, Ring, Gitter mit nz = 1) nach der Flächennormalen, sonst nach der LED:
//           vorne/hinten (Achse z): (x, y),  links/rechts (Achse x): (z, y),  oben/unten (Achse y): (x, z).
//         Eine Achse ohne Ausdehnung in der Bezugsbox ist selbst die Flächenachse (flache Wand: immer (x, y)).
#pragma once

#include <vector>

#include "whitehole/core/types.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/mapping/layout.hpp"

namespace whitehole::mapping {

/// Eine Achse: Wert -> [0,1]. `scale` ist die Länge, die auf 1 abgebildet wird; die Mitte der Achse landet bei 0,5.
/// Keine Ausdehnung (extent <= 0) ergibt 0.
[[nodiscard]] float normalizeAxis(float value, float low, float extent, float scale) noexcept;

/// Die Koordinaten p (z = 0) der LEDs von `fixture` bei Projection::Faces. `world` wird dafür nicht gebraucht.
[[nodiscard]] std::vector<Vec3> projectFaces(const Fixture& fixture, NormalizeMode mode);

/// Die Koordinaten p (z = 0) der LEDs von `fixture` bei Projection::Box. `world` sind ihre Raumpositionen (worldPositions),
/// `reference` die Box aller Box-Fixtures.
[[nodiscard]] std::vector<Vec3> projectBox(const Fixture& fixture, const std::vector<Vec3>& world, const Box& reference,
                                           NormalizeMode mode);

}  // namespace whitehole::mapping
