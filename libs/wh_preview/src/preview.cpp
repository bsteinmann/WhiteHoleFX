// preview.cpp: LED-Karte als Bild zeichnen.
//
// Ablauf: LEDs in die Bildebene projizieren -> Ausschnitt so wählen, dass alles mit Rand ins Bild
// passt -> von hinten nach vorn als weich begrenzte Scheiben zeichnen.
#include "whitehole/preview/preview.hpp"

#include "raster.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace whitehole::preview {

namespace {

using detail::GammaTable;
using detail::kMaxSide;
using detail::validGamma;

struct Projected {
    float x{0.0F};  // Bildebene, y nach oben
    float y{0.0F};
    float depth{0.0F};
};

Projected project(const Vec3& p, bool oblique) {
    if (!oblique) {
        return {p.x, p.y, p.z};
    }
    return {p.x + kObliqueShift * p.z, p.y + kObliqueShift * p.z, p.z};
}

/// Median des Abstands aufeinanderfolgender LEDs in pts[first, first + count) (höchstens 4096
/// Stichproben, gleichmäßig verteilt). Gibt `fallback` zurück, wenn alle Abstände 0 sind.
float medianNeighbourDistance(const std::vector<Projected>& pts, std::size_t first, std::size_t count,
                              float fallback) {
    if (count < 2 || first + count > pts.size()) {
        return fallback;
    }
    const std::size_t pairs = count - 1;
    const std::size_t step = std::max<std::size_t>(1, pairs / 4096);
    std::vector<float> distances;
    for (std::size_t i = first; i < first + pairs; i += step) {
        const float dx = pts[i + 1].x - pts[i].x;
        const float dy = pts[i + 1].y - pts[i].y;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d > 1e-6F) {
            distances.push_back(d);
        }
    }
    if (distances.empty()) {
        return fallback;
    }
    const auto middle = distances.begin() + static_cast<std::ptrdiff_t>(distances.size() / 2);
    std::nth_element(distances.begin(), middle, distances.end());
    return *middle;
}

}  // namespace

std::uint8_t toSrgb8(float linear) noexcept {
    const float v = std::isfinite(linear) ? std::clamp(linear, 0.0F, 1.0F) : 0.0F;
    const float s = v <= 0.0031308F ? v * 12.92F : 1.055F * std::pow(v, 1.0F / 2.4F) - 0.055F;
    return static_cast<std::uint8_t>(std::lround(std::clamp(s, 0.0F, 1.0F) * 255.0F));
}

float fromSrgb8(std::uint8_t value) noexcept {
    const float s = static_cast<float>(value) / 255.0F;
    return s <= 0.04045F ? s / 12.92F : std::pow((s + 0.055F) / 1.055F, 2.4F);
}

Result<Image> renderPreview(const mapping::LedMap& map, std::span<const ColorRgba> colors,
                            const PreviewOptions& options) {
    if (map.world.empty()) {
        return Error{ErrorCode::InvalidArgument, "Die LED-Karte ist leer"};
    }
    if (colors.size() != map.world.size()) {
        return Error{ErrorCode::InvalidArgument, "Die Zahl der Farben passt nicht zur LED-Karte"};
    }
    if (options.width == 0 || options.height == 0 || options.width > kMaxSide || options.height > kMaxSide ||
        options.margin * 2 + 2 > std::min(options.width, options.height)) {
        return Error{ErrorCode::InvalidArgument, "Ungültige Bildgröße oder zu großer Rand"};
    }

    // Tiefe vorhanden? Dann schräg projizieren.
    float zMin = map.world.front().z;
    float zMax = zMin;
    for (const Vec3& p : map.world) {
        zMin = std::min(zMin, p.z);
        zMax = std::max(zMax, p.z);
    }
    const bool oblique = options.frame ? options.frame->oblique : zMax - zMin > 1e-6F;

    std::vector<Projected> pts;
    pts.reserve(map.world.size());
    float minX = 0.0F;
    float maxX = 0.0F;
    float minY = 0.0F;
    float maxY = 0.0F;
    for (const Vec3& p : map.world) {
        const Projected q = project(p, oblique);
        if (pts.empty()) {
            minX = maxX = q.x;
            minY = maxY = q.y;
        }
        pts.push_back(q);
        minX = std::min(minX, q.x);
        maxX = std::max(maxX, q.x);
        minY = std::min(minY, q.y);
        maxY = std::max(maxY, q.y);
    }

    // Typischer LED-Abstand in Weltmaßen: Fläche je LED (bei Linien Länge je LED) als Grundwert.
    // Jedes Fixture verfeinert ihn mit dem Median des Abstands seiner Kabelnachbarn, damit ein
    // dichter Strip neben einer weitläufigen Matrix kleinere Punkte bekommt.
    const float spanX = maxX - minX;
    const float spanY = maxY - minY;
    const float count = static_cast<float>(pts.size());
    float areaSpacing = 1.0F;
    if (spanX > 1e-6F && spanY > 1e-6F) {
        areaSpacing = std::sqrt(spanX * spanY / count);
    } else if (std::max(spanX, spanY) > 1e-6F) {
        areaSpacing = std::max(spanX, spanY) / count;
    }
    std::vector<float> ledSpacing(pts.size(), areaSpacing);
    float widest = areaSpacing;
    for (std::size_t r = 0; r < map.ranges.size(); ++r) {
        const mapping::FixtureRange& range = map.ranges[r];
        if (static_cast<std::size_t>(range.first) + range.count > pts.size()) {
            continue;
        }
        // Bekannter Abstand der Form (nächste Nachbarin) vor dem Kabelabstand: so überlappen sich
        // die Punkte auch dann nicht, wenn die Matrix in x und y verschiedene Abstände hat.
        const float known = r < map.spacing.size() ? map.spacing[r] : 0.0F;
        const float spacing = known > 0.0F
                                  ? known
                                  : std::min(areaSpacing, medianNeighbourDistance(pts, range.first, range.count, areaSpacing));
        std::fill_n(ledSpacing.begin() + range.first, range.count, spacing);
        widest = std::max(widest, spacing);
    }
    // Der Ausschnitt richtet sich nach dem größten Punkt, damit nichts über den Rand ragt.
    const float spacing = widest;

    // Gamma je LED (nach Fixture), jeder Wert nur einmal als Tabelle.
    std::vector<GammaTable> tables;
    tables.emplace_back(validGamma(options.gamma));
    std::vector<std::uint16_t> ledTable(pts.size(), 0);
    for (std::size_t r = 0; r < map.ranges.size() && r < options.fixtureGamma.size(); ++r) {
        const mapping::FixtureRange& range = map.ranges[r];
        if (static_cast<std::size_t>(range.first) + range.count > pts.size()) {
            continue;
        }
        const float gamma = validGamma(options.fixtureGamma[r]);
        auto found = std::find_if(tables.begin(), tables.end(), [gamma](const GammaTable& t) { return t.gamma() == gamma; });
        if (found == tables.end()) {
            tables.emplace_back(gamma);
            found = tables.end() - 1;
        }
        std::fill_n(ledTable.begin() + range.first, range.count, static_cast<std::uint16_t>(found - tables.begin()));
    }

    // Maßstab: Der Rand bleibt frei; die LED-Mitten füllen den Rest, die Scheiben ragen
    // ein Stück über. Darum wird der halbe Abstand zusätzlich abgezogen.
    const float availW = static_cast<float>(options.width) - 2.0F * static_cast<float>(options.margin);
    const float availH = static_cast<float>(options.height) - 2.0F * static_cast<float>(options.margin);
    const float needW = spanX + spacing;
    const float needH = spanY + spacing;
    ViewFrame frame;
    frame.oblique = oblique;
    frame.minX = minX;
    frame.minY = minY;
    frame.scale = std::min(availW / needW, availH / needH);
    frame.offsetX = (static_cast<float>(options.width) - (spanX * frame.scale)) * 0.5F;
    frame.offsetY = (static_cast<float>(options.height) + (spanY * frame.scale)) * 0.5F;
    if (options.frame && std::isfinite(options.frame->scale) && options.frame->scale > 0.0F &&
        std::isfinite(options.frame->offsetX) && std::isfinite(options.frame->offsetY) &&
        std::isfinite(options.frame->minX) && std::isfinite(options.frame->minY)) {
        frame = *options.frame;
    }
    const float scale = frame.scale;
    const float offsetX = frame.offsetX;
    const float offsetY = frame.offsetY;
    minX = frame.minX;
    minY = frame.minY;

    Image image;
    image.frame = frame;
    detail::clearImage(image, options.width, options.height);

    // Hinten zuerst.
    std::vector<std::uint32_t> order(pts.size());
    std::iota(order.begin(), order.end(), 0U);
    if (oblique) {
        std::stable_sort(order.begin(), order.end(),
                         [&](std::uint32_t a, std::uint32_t b) { return pts[a].depth < pts[b].depth; });
    }

    for (const std::uint32_t index : order) {
        const float cx = offsetX + (pts[index].x - minX) * scale;
        const float cy = offsetY - (pts[index].y - minY) * scale;
        const float radius = std::clamp(options.dotScale * ledSpacing[index] * scale, 1.0F, 24.0F);
        const ColorRgba& c = colors[index];
        const float alpha = std::isfinite(c.a) ? std::clamp(c.a, 0.0F, 1.0F) : 1.0F;
        const GammaTable& table = tables[ledTable[index]];
        const std::array<float, 3> lit{static_cast<float>(table.byte(c.r * alpha)),
                                       static_cast<float>(table.byte(c.g * alpha)),
                                       static_cast<float>(table.byte(c.b * alpha))};

        detail::drawDisc(image, cx, cy, radius, lit);
    }
    return image;
}

std::vector<ColorRgba> layoutColors(const mapping::LedMap& map) {
    std::vector<ColorRgba> colors(map.world.size(), ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    const float golden = 0.61803398875F;
    float hue = 0.1F;
    for (const mapping::FixtureRange& range : map.ranges) {
        hue = std::fmod(hue + golden, 1.0F);
        // HSV (S = 0.8) -> RGB; Helligkeit wächst entlang des Kabels von 0,25 auf 1.
        const float h6 = hue * 6.0F;
        const int sector = static_cast<int>(h6);
        const float f = h6 - static_cast<float>(sector);
        const float s = 0.8F;
        const float p = 1.0F - s;
        const float q = 1.0F - s * f;
        const float t = 1.0F - s * (1.0F - f);
        float r = 1.0F;
        float g = t;
        float b = p;
        switch (sector % 6) {
            case 1: r = q; g = 1.0F; b = p; break;
            case 2: r = p; g = 1.0F; b = t; break;
            case 3: r = p; g = q; b = 1.0F; break;
            case 4: r = t; g = p; b = 1.0F; break;
            case 5: r = 1.0F; g = p; b = q; break;
            default: break;
        }
        for (std::uint32_t i = 0; i < range.count && range.first + i < colors.size(); ++i) {
            const float along = range.count > 1 ? static_cast<float>(i) / static_cast<float>(range.count - 1) : 1.0F;
            const float v = 0.25F + 0.75F * along;
            colors[range.first + i] = ColorRgba{r * v, g * v, b * v, 1.0F};
        }
    }
    return colors;
}

std::vector<ColorRgba> stageColors(const mapping::LedMap& map, std::span<const mapping::FixtureId> selected) {
    std::vector<ColorRgba> colors = layoutColors(map);
    for (const mapping::FixtureRange& range : map.ranges) {
        if (range.count == 0 || static_cast<std::size_t>(range.first) + range.count > colors.size()) {
            continue;
        }
        const bool keep = selected.empty() || std::find(selected.begin(), selected.end(), range.fixture) != selected.end();
        for (std::uint32_t i = 0; i < range.count; ++i) {
            ColorRgba& c = colors[range.first + i];
            if (!keep) {
                c = ColorRgba{c.r * 0.25F, c.g * 0.25F, c.b * 0.25F, 1.0F};
            }
        }
        if (keep) {
            colors[range.first] = ColorRgba{1.0F, 1.0F, 1.0F, 1.0F};  // Anfang des Strangs
        }
    }
    return colors;
}

}  // namespace whitehole::preview
