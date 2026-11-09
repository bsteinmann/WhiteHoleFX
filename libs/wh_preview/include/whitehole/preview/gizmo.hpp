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
// Drehringe: Um jede Achse liegt ein Kreis (Ebene senkrecht zur Achse, Mittelpunkt am Fixture). Greift man einen Ring,
// bestimmt die Maus den Winkel auf diesem Kreis; die Änderung dieses Winkels ist die Drehung um die Weltachse. Ein Ring,
// den man fast von der Kante sieht (flache Ellipse), ist nicht greifbar, wie ein zur Kamera zeigender Pfeil.
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
inline constexpr int kGizmoRingSegments = 48;           // Kreis als Vieleck mit so vielen Seiten
inline constexpr float kGizmoRingRadius = 0.7F;         // Ringradius im Verhältnis zur Pfeillänge
inline constexpr float kGizmoRingMinMinorPixels = 6.0F; // Ringe mit kleinerer Halbachse im Bild (Kantenansicht) sind nicht greifbar

/// Die drei Pfeile im Bild (Bildpunkte, Zeile 0 oben).
struct GizmoGeometry {
    float originX{0.0F};
    float originY{0.0F};
    std::array<float, 3> tipX{};  // Spitze je Achse (x, y, z)
    std::array<float, 3> tipY{};
    float worldLength{1.0F};      // Länge eines Pfeils in Welteinheiten
    /// Ring je Achse: kGizmoRingSegments + 1 Punkte (erster = letzter); Winkel 0 liegt auf der ersten Ebenenachse
    /// (x: y, y: z, z: x) und wächst im Rechtssinn um die Achse.
    std::array<std::array<float, kGizmoRingSegments + 1>, 3> ringX{};
    std::array<std::array<float, kGizmoRingSegments + 1>, 3> ringY{};
    std::array<bool, 3> ringVisible{};  // false, wenn ein Punkt des Rings hinter der Kamera liegt
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

/// Kleine Halbachse des Rings der Achse `axis` im Bild (Bildpunkte); 0 bei unsichtbarem Ring.
[[nodiscard]] float gizmoRingMinorPixels(const GizmoGeometry& gizmo, int axis) noexcept;

/// Der Ring, dessen Linie dem Punkt (x, y) am nächsten liegt, höchstens `tolerance` Bildpunkte entfernt; sonst leer.
/// Unsichtbare und von der Kante gesehene Ringe zählen nicht.
[[nodiscard]] std::optional<int> pickGizmoRing(const GizmoGeometry& gizmo, float x, float y, float tolerance = 7.0F) noexcept;

/// Winkel (Grad, 0 bis unter 360) des Punkts auf dem Ring der Achse `axis`, der (x, y) am nächsten liegt.
[[nodiscard]] float gizmoRingAngle(const GizmoGeometry& gizmo, int axis, float x, float y) noexcept;

/// Winkelunterschied `to - from` in Grad, auf (-180, 180] gebracht (Ziehen über die 0°/360°-Grenze).
[[nodiscard]] float angleDelta(float from, float to) noexcept;

/// Rastet auf Vielfache von `step` (zum Beispiel 0,1) ein. `step` <= 0: unverändert.
[[nodiscard]] float snapDistance(float value, float step) noexcept;

}  // namespace whitehole::preview
