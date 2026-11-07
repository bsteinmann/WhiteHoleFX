// gizmo.hpp: Achsen-Griff (Gizmo) für die 3D-Bühnenansicht: Geometrie, Treffer und Ziehen.
//
// Warum es diese Datei gibt: Im Layout will man ein Fixture direkt in der 3D-Ansicht verschieben, so wie
// in einem 3D-Programm: am Fixture stehen drei Pfeile (x rot, y grün, z blau), man fasst einen mit der Maus und
// zieht. Die Rechnung dahinter hat nichts mit Qt zu tun und steht deshalb hier, damit sie sich testen lässt:
// Wo liegen die Pfeilspitzen im Bild, welcher Pfeil liegt unter der Maus, und wie viele Welteinheiten entsprechen
// einer Mausbewegung in Bildpunkten entlang dieses Pfeils.
//
// Die Pfeile haben eine feste Länge in Bildpunkten (egal wie weit weg das Fixture liegt) und laufen in
// Richtung der Weltachsen. Zeigt eine Achse fast auf die Kamera zu (kurzer Pfeil im Bild), ist sie nicht greifbar:
// ein Zug in der Bildebene sagt dann nichts über die Tiefe.
//
// Abhängigkeiten: wh_core, view3d.hpp. Kein Qt, keine GPU.
#pragma once

#include <array>
#include <optional>

#include "whitehole/core/types.hpp"
#include "whitehole/preview/view3d.hpp"

namespace whitehole::preview {

inline constexpr float kGizmoLengthPixels = 70.0F;      // Länge eines Pfeils im Bild
inline constexpr float kGizmoMinPixels = 12.0F;         // kürzere Pfeile (Achse zeigt zur Kamera) lassen sich nicht greifen

/// Die drei Pfeile im Bild (Bildpunkte, Zeile 0 oben).
struct GizmoGeometry {
    float originX{0.0F};
    float originY{0.0F};
    std::array<float, 3> tipX{};  // Spitze je Achse (x, y, z)
    std::array<float, 3> tipY{};
    float worldLength{1.0F};      // Länge eines Pfeils in Welteinheiten
};

/// Pfeile für `anchor` (Raumpunkt am Fixture). Leer, wenn der Punkt hinter der Kamera liegt.
[[nodiscard]] std::optional<GizmoGeometry> gizmoFor(const Projector& projector, const Vec3& anchor,
                                                    float lengthPixels = kGizmoLengthPixels);

/// Bildlänge des Pfeils der Achse `axis` (0 x, 1 y, 2 z).
[[nodiscard]] float gizmoAxisPixels(const GizmoGeometry& gizmo, int axis) noexcept;

/// Der Pfeil, dessen Linie dem Punkt (x, y) am nächsten liegt, höchstens `tolerance` Bildpunkte entfernt; sonst leer.
/// Zu kurze Pfeile (kGizmoMinPixels) zählen nicht.
[[nodiscard]] std::optional<int> pickGizmoAxis(const GizmoGeometry& gizmo, float x, float y, float tolerance = 8.0F) noexcept;

/// Wie viele Welteinheiten entlang der Achse `axis` eine Mausbewegung um (dx, dy) Bildpunkte bedeutet: der Anteil der
/// Bewegung in Richtung des Pfeils, im Maßstab des Pfeils. 0 bei zu kurzem Pfeil.
[[nodiscard]] float gizmoDragDistance(const GizmoGeometry& gizmo, int axis, float dx, float dy) noexcept;

/// Rastet auf Vielfache von `step` (zum Beispiel 0,1) ein. `step` <= 0: unverändert.
[[nodiscard]] float snapDistance(float value, float step) noexcept;

}  // namespace whitehole::preview
