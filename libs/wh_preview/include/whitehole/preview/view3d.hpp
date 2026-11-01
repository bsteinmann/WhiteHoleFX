// view3d.hpp: frei drehbare 3D-Ansicht der LED-Karte.
//
// Warum es diese Datei gibt: Bei einer hängenden Installation, einem Cube oder einem Vorhang aus
// Strips reicht die Sicht von vorn nicht: man will um das Ganze herumgehen und sehen, wie ein
// Effekt durch den Raum läuft. Die 3D-Vorschau zeichnet dieselben LEDs und Farben wie die 2D-Vorschau,
// aber durch eine Kamera, die man um die Mitte der Installation dreht (Orbit), verschiebt und
// zoomt. Sie kennt keine Fixture-Formen: jede LED ist nur ein Punkt im Raum, darum funktioniert sie
// für Matrizen, Strips, Punktwolken und Mischungen gleichermaßen.
//
// Koordinaten: wie im Layout (x nach rechts, y nach oben, z zum Betrachter, rechtshändig). Mit
// yaw = 0 und pitch = 0 sieht die Kamera genau die Ansicht von vorn.
//
// Abhängigkeiten: wh_core, wh_mapping. Kein Betriebssystem, keine GPU, kein Qt.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/mapping/led_map.hpp"
#include "whitehole/preview/preview.hpp"

namespace whitehole::preview {

/// Feste Blickrichtungen für die Knöpfe der Oberfläche.
enum class ViewPreset { Front, Side, Top, Perspective };

struct Camera3d {
    float yawDegrees{35.0F};    // Drehung um die senkrechte Achse; 0 = von vorn, 90 = von rechts
    float pitchDegrees{22.0F};  // Neigung; 0 = waagerecht, 90 = von oben
    float zoom{1.0F};           // 1 = die ganze Installation passt ins Bild
    float panX{0.0F};           // Verschiebung des Bildes in Bruchteilen der kürzeren Bildseite
    float panY{0.0F};           // (positiv: Bild nach rechts / nach oben)
    bool perspective{true};     // false: Parallelprojektion (Front/Seite/Oben sind maßhaltig)
    bool showBox{true};         // Rahmen um die Installation zeichnen (Orientierung)
    float depthCue{0.35F};      // wie stark entfernte LEDs abdunkeln (0 = gar nicht, 1 = bis Hintergrund)

    bool operator==(const Camera3d&) const = default;
};

inline constexpr float kMinZoom3d = 0.1F;
inline constexpr float kMaxZoom3d = 40.0F;

/// Kamera für eine feste Ansicht (Zoom und Verschiebung zurückgesetzt).
[[nodiscard]] Camera3d cameraFor(ViewPreset preset) noexcept;

/// Kamera um `deltaYaw` / `deltaPitch` (Grad) gedreht. Yaw bleibt in (-180, 180], Pitch in [-90, 90].
[[nodiscard]] Camera3d orbited(const Camera3d& camera, float deltaYaw, float deltaPitch) noexcept;

/// Zoom mit `factor` multipliziert, auf [kMinZoom3d, kMaxZoom3d] begrenzt.
[[nodiscard]] Camera3d zoomed(const Camera3d& camera, float factor) noexcept;

/// Bild um (dx, dy) Bruchteile der kürzeren Bildseite verschoben.
[[nodiscard]] Camera3d panned(const Camera3d& camera, float dx, float dy) noexcept;

/// Box und Kugel um alle LED-Positionen (für Projector). Leere Karte: Mitte 0, Radius 1.
struct Bounds3d {
    Vec3 center;
    float radius{1.0F};  // Kugel um die Box (halbe Diagonale, mindestens > 0)
    Vec3 min;
    Vec3 max;
};
[[nodiscard]] Bounds3d boundsOf(const mapping::LedMap& map) noexcept;

/// Wo ein Raumpunkt im Bild landet.
struct ScreenPoint {
    float x{0.0F};             // Pixel, rechts
    float y{0.0F};             // Pixel, Zeile 0 oben
    float depth{0.0F};         // Abstand vor der Kamera (größer = weiter weg)
    float pixelsPerUnit{1.0F}; // Maßstab an dieser Stelle (Pixel je Welteinheit)
};

/// Rechnet Raumpunkte für eine Kamera, einen Bildausschnitt und eine Installation in Bildpunkte um.
class Projector {
public:
    /// Perspektive: die Kugel um die Installation passt ins Bild (stabil beim Drehen). Parallelprojektion:
    /// die Box füllt das Bild (feste Ansichten Front/Seite/Oben nutzen den Platz aus).
    Projector(const Camera3d& camera, const Bounds3d& bounds, std::uint32_t width, std::uint32_t height,
              std::uint32_t margin);

    /// Leer, wenn der Punkt hinter der Kamera liegt.
    [[nodiscard]] std::optional<ScreenPoint> project(const Vec3& world) const noexcept;

    /// Nächster und fernster Abstand der Kugel um die Installation (für den Tiefenhinweis).
    [[nodiscard]] float nearDepth() const noexcept { return distance_ - radius_; }
    [[nodiscard]] float farDepth() const noexcept { return distance_ + radius_; }

private:
    Vec3 center_;
    float radius_{1.0F};
    float distance_{1.0F};      // Abstand Kamera - Mittelpunkt
    Vec3 right_;
    Vec3 up_;
    Vec3 toCamera_;             // vom Mittelpunkt zur Kamera (Einheitsvektor)
    bool perspective_{true};
    float scale_{1.0F};         // Parallel: Pixel je Einheit; perspektivisch: Brennweite in Pixeln
    float cx_{0.0F};
    float cy_{0.0F};
};

/// Zeichnet die LEDs mit den gegebenen Farben (linear, eine je LED) durch die Kamera. Ein
/// `frame` in den Optionen wird ignoriert (die Kamera bestimmt den Ausschnitt), der gemeldete
/// Rahmen des Bildes bleibt leer. Fehler wie renderPreview.
[[nodiscard]] Result<Image> renderPreview3d(const mapping::LedMap& map, std::span<const ColorRgba> colors,
                                            const Camera3d& camera, const PreviewOptions& options = {});

}  // namespace whitehole::preview
