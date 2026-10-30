// preview.hpp: zeigt, was die LEDs gerade tun, als Bild.
//
// Warum es diese Datei gibt: Beim Einrichten will man sehen, ob das Layout stimmt (liegt die
// Matrix richtig herum, in welcher Richtung läuft der Strip?) und ob der Effekt aussieht wie
// gedacht, ohne dass Hardware angeschlossen ist. Die Vorschau zeichnet jede LED als Punkt an
// ihrer Position; die Farbe kommt aus demselben Farbpuffer, der auch zur Ausgabe geht.
//
// Ansicht: Von vorn (x nach rechts, y nach oben). Hat das Layout Tiefe (z), wird schräg von
// vorn oben projiziert, damit ein Cube nicht zu einem Quadrat zusammenfällt; weiter hinten
// liegende LEDs werden zuerst gezeichnet und von näheren verdeckt.
//
// Farben: Die Eingabe ist linear (wie im Farbpuffer). Das Bild ist sRGB, wie ein Bildschirm es
// erwartet. Eine ausgeschaltete LED erscheint als dunkler Punkt, damit man das Layout auch bei
// schwarzem Bild sieht.
//
// Abhängigkeiten: wh_core, wh_mapping. Kein Betriebssystem, keine GPU.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/mapping/led_map.hpp"

namespace whitehole::preview {

/// 8 Bit je Kanal, RGBA, Zeile 0 oben.
struct Image {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba;
};

struct PreviewOptions {
    std::uint32_t width{960};
    std::uint32_t height{540};
    std::uint32_t margin{24};   // Rand in Pixeln
    float dotScale{0.45F};      // Punktradius als Anteil des typischen LED-Abstands
};

/// Zeichnet die LEDs mit den gegebenen Farben (linear, eine je LED).
/// Fehler: InvalidArgument (Farbenzahl passt nicht zur Karte, Bild kleiner als 2 Rand,
/// Größe 0 oder über 16384, leere Karte).
[[nodiscard]] Result<Image> renderPreview(const mapping::LedMap& map, std::span<const ColorRgba> colors,
                                          const PreviewOptions& options = {});

/// Farben, die die Verkabelung sichtbar machen: jedes Fixture bekommt einen eigenen Farbton, und
/// entlang des Kabels wird es heller. Die erste LED am Kabel ist also die dunkelste.
[[nodiscard]] std::vector<ColorRgba> layoutColors(const mapping::LedMap& map);

/// Linear -> sRGB (0..255), mit Begrenzung auf den gültigen Bereich.
[[nodiscard]] std::uint8_t toSrgb8(float linear) noexcept;

/// sRGB (0..255) -> linear (0..1); die Umkehrung von toSrgb8, z. B. für Farbwähler.
[[nodiscard]] float fromSrgb8(std::uint8_t value) noexcept;

}  // namespace whitehole::preview
