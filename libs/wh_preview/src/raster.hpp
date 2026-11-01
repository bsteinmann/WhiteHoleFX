// raster.hpp: gemeinsame Zeichenbausteine der 2D- und der 3D-Vorschau (nur innerhalb von wh_preview).
//
// Warum es diese Datei gibt: renderPreview (Ansicht von vorn) und renderPreview3d (frei drehbar)
// zeichnen LEDs als weich begrenzte Scheiben mit demselben Gamma und denselben Farben für "aus".
// Damit beide Ansichten dieselbe LED gleich aussehen lassen, steht das Zeichnen hier einmal.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "whitehole/preview/preview.hpp"

namespace whitehole::preview::detail {

inline constexpr std::uint32_t kMaxSide = 16384;
inline constexpr std::array<std::uint8_t, 3> kBackground{14, 14, 18};
inline constexpr std::array<std::uint8_t, 3> kOffLed{34, 34, 40};

inline float blend(float dst, float src, float coverage) { return dst + (src - dst) * coverage; }

/// Wert (0..1) -> Bildschirm-Byte für ein Gamma: sRGB(v^gamma). Eine Tabelle spart pow() je LED.
class GammaTable {
public:
    explicit GammaTable(float gamma) : gamma_{gamma} {
        for (std::size_t i = 0; i < bytes_.size(); ++i) {
            const double v = static_cast<double>(i) / static_cast<double>(bytes_.size() - 1);
            bytes_[i] = toSrgb8(static_cast<float>(std::pow(v, static_cast<double>(gamma))));
        }
    }
    [[nodiscard]] float gamma() const noexcept { return gamma_; }
    [[nodiscard]] std::uint8_t byte(float v) const noexcept {
        const float c = std::isfinite(v) ? std::clamp(v, 0.0F, 1.0F) : 0.0F;
        return bytes_[static_cast<std::size_t>(std::lround(c * static_cast<float>(bytes_.size() - 1)))];
    }

private:
    float gamma_;
    std::array<std::uint8_t, 4097> bytes_{};
};

inline float validGamma(float gamma) noexcept {
    return std::isfinite(gamma) && gamma >= 0.1F && gamma <= 10.0F ? gamma : 2.2F;
}

/// Bild mit dem Hintergrund füllen (und Größe setzen).
inline void clearImage(Image& image, std::uint32_t width, std::uint32_t height) {
    image.width = width;
    image.height = height;
    image.rgba.resize(std::size_t{width} * height * 4);
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        image.rgba[i] = kBackground[0];
        image.rgba[i + 1] = kBackground[1];
        image.rgba[i + 2] = kBackground[2];
        image.rgba[i + 3] = 255;
    }
}

/// Eine Scheibe mit weichem Rand zeichnen. `lit` sind die Bildschirm-Bytes (0..255) der LED,
/// `fade` (0..1) dimmt sie Richtung Hintergrund (Tiefenhinweis der 3D-Ansicht).
inline void drawDisc(Image& image, float cx, float cy, float radius, const std::array<float, 3>& lit, float fade = 1.0F,
                     float offScale = 1.0F) {
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - radius - 1.0F)));
    const int x1 = std::min(static_cast<int>(image.width) - 1, static_cast<int>(std::ceil(cx + radius + 1.0F)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - radius - 1.0F)));
    const int y1 = std::min(static_cast<int>(image.height) - 1, static_cast<int>(std::ceil(cy + radius + 1.0F)));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5F - cx;
            const float dy = static_cast<float>(y) + 0.5F - cy;
            const float coverage = std::clamp(radius + 0.5F - std::sqrt(dx * dx + dy * dy), 0.0F, 1.0F);
            if (coverage <= 0.0F) {
                continue;
            }
            std::uint8_t* px = &image.rgba[(static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4];
            for (std::size_t ch = 0; ch < 3; ++ch) {
                // Eine ausgeschaltete LED bleibt als dunkler Punkt sichtbar: 0 wird zum Grauwert
                // der "aus"-LED, 255 bleibt 255.
                const float off = static_cast<float>(kOffLed[ch]) * offScale;
                float target = off + lit[ch] * (1.0F - off / 255.0F);
                target = blend(static_cast<float>(kBackground[ch]), target, fade);
                px[ch] = static_cast<std::uint8_t>(std::lround(blend(static_cast<float>(px[ch]), target, coverage)));
            }
        }
    }
}

/// Scheibe mit Schein, die auf das Bild *addiert* wird (3D-Ansicht): Dunkle LEDs verdecken nichts, und
/// man sieht durch einen Cube hindurch. Der Kern ist die Scheibe selbst, darum herum fällt ein Hof
/// bis etwa zum 2,5-fachen Radius auf null ab.
inline void drawGlow(Image& image, float cx, float cy, float radius, const std::array<float, 3>& lit, float fade) {
    // Eine LED, die nichts addiert (schwarz oder ganz ausgeblendet), ändert kein Pixel: sofort fertig.
    const float brightest = std::max({lit[0], lit[1], lit[2]});
    if (!(brightest * fade > 0.0F)) {
        return;
    }
    const float reach = radius * 2.5F + 1.0F;
    const float reachSq = reach * reach;
    const float falloff = std::max(reach - radius, 1e-3F);
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - reach)));
    const int x1 = std::min(static_cast<int>(image.width) - 1, static_cast<int>(std::ceil(cx + reach)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - reach)));
    const int y1 = std::min(static_cast<int>(image.height) - 1, static_cast<int>(std::ceil(cy + reach)));
    for (int y = y0; y <= y1; ++y) {
        const float dy = static_cast<float>(y) + 0.5F - cy;
        const float dySq = dy * dy;
        if (dySq >= reachSq) {
            continue;  // ganze Zeile außerhalb des Hofs
        }
        std::uint8_t* px = &image.rgba[(static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x0)) * 4];
        for (int x = x0; x <= x1; ++x, px += 4) {
            const float dx = static_cast<float>(x) + 0.5F - cx;
            const float distSq = dx * dx + dySq;
            if (distSq >= reachSq) {
                continue;  // jenseits des Hofs ist das Gewicht null: keine Wurzel nötig
            }
            const float d = std::sqrt(distSq);
            const float core = std::clamp(radius + 0.5F - d, 0.0F, 1.0F);
            const float t = std::clamp((d - radius) / falloff, 0.0F, 1.0F);
            const float halo = 0.3F * (1.0F - t) * (1.0F - t);
            const float weight = std::max(core, halo) * fade;
            if (weight <= 0.0F) {
                continue;
            }
            for (std::size_t ch = 0; ch < 3; ++ch) {
                // Summe ist nie negativ: Abschneiden von (Wert + 0,5) rundet wie lround, ist aber viel billiger.
                const float sum = std::min(255.0F, static_cast<float>(px[ch]) + lit[ch] * weight);
                px[ch] = static_cast<std::uint8_t>(static_cast<int>(sum + 0.5F));
            }
        }
    }
}

/// Scheibe in einer festen Farbe, die nur aufhellt (Maximum statt Überdecken): für ausgeschaltete LEDs
/// im additiven Modus, damit sie leuchtende Nachbarn nicht abdunkeln.
inline void drawDiscMax(Image& image, float cx, float cy, float radius, const std::array<float, 3>& color) {
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - radius - 1.0F)));
    const int x1 = std::min(static_cast<int>(image.width) - 1, static_cast<int>(std::ceil(cx + radius + 1.0F)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - radius - 1.0F)));
    const int y1 = std::min(static_cast<int>(image.height) - 1, static_cast<int>(std::ceil(cy + radius + 1.0F)));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5F - cx;
            const float dy = static_cast<float>(y) + 0.5F - cy;
            const float coverage = std::clamp(radius + 0.5F - std::sqrt(dx * dx + dy * dy), 0.0F, 1.0F);
            if (coverage <= 0.0F) {
                continue;
            }
            std::uint8_t* px = &image.rgba[(static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4];
            for (std::size_t ch = 0; ch < 3; ++ch) {
                const float value = blend(static_cast<float>(px[ch]), color[ch], coverage);
                px[ch] = static_cast<std::uint8_t>(std::lround(std::max(static_cast<float>(px[ch]), value)));
            }
        }
    }
}

}  // namespace whitehole::preview::detail
