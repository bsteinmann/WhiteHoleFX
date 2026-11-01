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
inline void drawDisc(Image& image, float cx, float cy, float radius, const std::array<float, 3>& lit, float fade = 1.0F) {
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
                const float off = static_cast<float>(kOffLed[ch]);
                float target = off + lit[ch] * (1.0F - off / 255.0F);
                target = blend(static_cast<float>(kBackground[ch]), target, fade);
                px[ch] = static_cast<std::uint8_t>(std::lround(blend(static_cast<float>(px[ch]), target, coverage)));
            }
        }
    }
}

}  // namespace whitehole::preview::detail
