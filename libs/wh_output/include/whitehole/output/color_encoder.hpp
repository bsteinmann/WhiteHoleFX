// color_encoder.hpp: macht aus einer Float-Farbe die drei Bytes einer LED.
//
// Warum es diese Datei gibt: Die Effekte liefern Farben als Float-RGBA (0..1). Eine LED
// erwartet 8-Bit-Werte in ihrer eigenen Kanalreihenfolge, und ihr Licht ist proportional
// zum Wert, während das Auge dunkle Töne feiner unterscheidet: Ein Verlauf von 0 bis 1
// wirkt auf der LED zu hell und springt im Dunkeln. Gamma-Korrektur gleicht das aus.
// Diese Datei ist der eine Ort, an dem aus "Effekt-Farbe" "Hardware-Bytes" werden.
//
// Konvention: Effekt-Werte sind so gemeint, wie man sie auf einem Bildschirm sieht
// (wahrnehmungsnah). Die LED strahlt linear, daher wird potenziert: out = v^gamma.
// Gamma 1 lässt die Werte unverändert; 2,2 ist ein üblicher Startwert für WS28xx.
//
// Aufgabe (pro LED):
//   1. Alpha: Die Farbe liegt über Schwarz, also rgb * a.
//   2. Helligkeit: mit `brightness` (0..1) multiplizieren.
//   3. Gamma: out = v^gamma.
//   4. Auf 8 Bit runden und in die Kanalreihenfolge der LED legen (z. B. GRB).
//
// Umsetzung: Eine Tabelle mit 65536 Einträgen (Eingang in 16 Bit) vermeidet pow() pro
// Pixel. Bei 16 Bit Eingang bleibt auch im Dunkeln jeder 8-Bit-Schritt erreichbar.
// NaN und negative Werte ergeben 0, Werte über 1 werden auf 1 begrenzt.
//
// Nicht enthalten (späteres Release): Weißkanal (RGBW), Dithering, Farbkalibrierung je Kanal.
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"

namespace whitehole::output {

enum class ColorOrder : std::uint8_t { RGB, RBG, GRB, GBR, BRG, BGR };

struct ColorSettings {
    ColorOrder order{ColorOrder::RGB};
    float gamma{2.2F};       // 0,1 bis 10
    float brightness{1.0F};  // 0 bis 1

    bool operator==(const ColorSettings&) const = default;
};

class ColorEncoder {
public:
    /// Fehler: InvalidArgument bei Gamma außerhalb von [0,1; 10], Helligkeit außerhalb
    /// von [0; 1] oder nicht endlichen Zahlen.
    [[nodiscard]] static Result<ColorEncoder> create(const ColorSettings& settings);

    /// Schreibt die drei Bytes der LED in der gewählten Kanalreihenfolge.
    [[nodiscard]] std::array<std::uint8_t, 3> encode(const ColorRgba& color) const noexcept;

    [[nodiscard]] const ColorSettings& settings() const noexcept { return settings_; }

private:
    ColorEncoder(const ColorSettings& settings, std::vector<std::uint8_t> table)
        : settings_{settings}, table_{std::move(table)} {}

    [[nodiscard]] std::uint8_t channel(float linear) const noexcept;

    ColorSettings settings_;
    std::vector<std::uint8_t> table_;  // 65536 Einträge
};

}  // namespace whitehole::output
