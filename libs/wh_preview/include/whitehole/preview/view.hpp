// view.hpp: Abbildung zwischen Weltkoordinaten und Bildpunkten der Vorschau, plus Treffertests.
//
// Warum es diese Datei gibt: Die Vorschau soll nicht nur zeigen, sondern auch bedienbar sein
// (Fixtures anklicken und ziehen, den Effektbereich als Rahmen einzeichnen). Dafür muss die
// Oberfläche wissen, wo ein Punkt der Welt im Bild landet und umgekehrt. renderPreview meldet den
// Ausschnitt, den es gewählt hat, als ViewFrame; Rechnungen mit dem Rahmen stehen hier und sind
// ohne Oberfläche testbar. Wird ein Rahmen in die PreviewOptions gelegt, nutzt das Bild genau
// diesen Ausschnitt: so bleibt die Ansicht beim Ziehen stehen, auch wenn sich die Box der
// Fixtures ändert.
//
// Ansicht: x nach rechts, y nach oben; bei Tiefe (z) schräg projiziert (siehe preview.hpp).
#pragma once

#include <optional>
#include <vector>

#include "whitehole/core/types.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/mapping/layout.hpp"

namespace whitehole::preview {

/// Wie weit z das Bild nach rechts oben schiebt (Anteil von z).
inline constexpr float kObliqueShift = 0.35F;

struct ViewFrame {
    bool oblique{false};
    float minX{0.0F};     // kleinster projizierter Wert (Bildebene), entspricht offsetX
    float minY{0.0F};
    float scale{1.0F};    // Pixel je Welteinheit
    float offsetX{0.0F};  // Pixel
    float offsetY{0.0F};  // Pixel der Zeile, in der minY liegt (y wächst im Bild nach unten)

    bool operator==(const ViewFrame&) const = default;
};

struct PixelPoint {
    float x{0.0F};
    float y{0.0F};
};

struct PixelRect {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};

    [[nodiscard]] float width() const noexcept { return right - left; }
    [[nodiscard]] float height() const noexcept { return bottom - top; }
};

/// Bildpunkt eines Weltpunkts.
[[nodiscard]] PixelPoint toPixel(const ViewFrame& frame, const Vec3& world) noexcept;

/// Weltpunkt in der Ebene `z` unter einem Bildpunkt (Umkehrung von toPixel für festes z).
[[nodiscard]] Vec3 toWorld(const ViewFrame& frame, float pixelX, float pixelY, float z = 0.0F) noexcept;

/// Umschließendes Rechteck einer Weltbox im Bild.
[[nodiscard]] PixelRect toPixels(const ViewFrame& frame, const mapping::Box& box) noexcept;

/// Die Box eines Fixtures im Raum (Ursprung + lokale Box).
[[nodiscard]] mapping::Box worldBox(const mapping::Fixture& fixture) noexcept;

/// Das Fixture unter dem Bildpunkt (Toleranz in Pixeln um die Box). Liegen mehrere übereinander,
/// gewinnt das mit der kleinsten Fläche, bei Gleichstand das spätere in der Liste. Leer: kein Treffer.
[[nodiscard]] std::optional<mapping::FixtureId> fixtureAt(const mapping::Layout& layout, const ViewFrame& frame, float pixelX,
                                                 float pixelY, float tolerance = 4.0F);

/// Alle Fixtures, deren Bildrechteck das Rechteck berührt (für den Gummiband-Rahmen), in
/// Listenreihenfolge.
[[nodiscard]] std::vector<mapping::FixtureId> fixturesInRect(const mapping::Layout& layout, const ViewFrame& frame,
                                                    const PixelRect& rect);

}  // namespace whitehole::preview
