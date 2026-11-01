// view3d.cpp: Kamera, Projektion und Zeichnen der 3D-Vorschau.
//
// Ablauf: Kugel um alle LEDs bestimmen -> Kamera darauf ausrichten (Orbit um die Mitte) -> jede
// LED projizieren -> von hinten nach vorn als Scheiben zeichnen, entfernte leicht abgedunkelt.
#include "whitehole/preview/view3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

#include "raster.hpp"

namespace whitehole::preview {

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kHalfFov = 17.5F * kPi / 180.0F;  // halber vertikaler Öffnungswinkel (Perspektive)
constexpr float kFit = 1.08F;                     // Luft um die Kugel

float radians(float degrees) noexcept { return degrees * kPi / 180.0F; }
float dot(const Vec3& a, const Vec3& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 sub(const Vec3& a, const Vec3& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float length(const Vec3& a) noexcept { return std::sqrt(dot(a, a)); }

float finiteOr(float value, float fallback) noexcept { return std::isfinite(value) ? value : fallback; }

/// Abstand benachbarter LEDs je Bereich (Welteinheiten): bekannter Abstand, sonst Median der Kabelnachbarn.
float rangeSpacing(const mapping::LedMap& map, std::size_t r, float fallback) {
    const float known = r < map.spacing.size() ? map.spacing[r] : 0.0F;
    if (known > 0.0F) {
        return known;
    }
    const mapping::FixtureRange& range = map.ranges[r];
    if (range.count < 2 || static_cast<std::size_t>(range.first) + range.count > map.world.size()) {
        return fallback;
    }
    const std::size_t pairs = range.count - 1;
    const std::size_t step = std::max<std::size_t>(1, pairs / 4096);
    std::vector<float> distances;
    for (std::size_t i = range.first; i < range.first + pairs; i += step) {
        const float d = length(sub(map.world[i + 1], map.world[i]));
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

Camera3d cameraFor(ViewPreset preset) noexcept {
    Camera3d camera;
    switch (preset) {
        case ViewPreset::Front:
            camera.yawDegrees = 0.0F;
            camera.pitchDegrees = 0.0F;
            camera.perspective = false;
            break;
        case ViewPreset::Side:
            camera.yawDegrees = 90.0F;
            camera.pitchDegrees = 0.0F;
            camera.perspective = false;
            break;
        case ViewPreset::Top:
            camera.yawDegrees = 0.0F;
            camera.pitchDegrees = 90.0F;
            camera.perspective = false;
            break;
        case ViewPreset::Perspective:
            break;
    }
    return camera;
}

Camera3d orbited(const Camera3d& camera, float deltaYaw, float deltaPitch) noexcept {
    Camera3d next = camera;
    float yaw = std::fmod(camera.yawDegrees + finiteOr(deltaYaw, 0.0F), 360.0F);
    if (yaw > 180.0F) {
        yaw -= 360.0F;
    } else if (yaw <= -180.0F) {
        yaw += 360.0F;
    }
    next.yawDegrees = yaw;
    next.pitchDegrees = std::clamp(camera.pitchDegrees + finiteOr(deltaPitch, 0.0F), -90.0F, 90.0F);
    return next;
}

Camera3d zoomed(const Camera3d& camera, float factor) noexcept {
    Camera3d next = camera;
    if (std::isfinite(factor) && factor > 0.0F) {
        next.zoom = std::clamp(camera.zoom * factor, kMinZoom3d, kMaxZoom3d);
    }
    return next;
}

Camera3d panned(const Camera3d& camera, float dx, float dy) noexcept {
    Camera3d next = camera;
    next.panX = std::clamp(camera.panX + finiteOr(dx, 0.0F), -4.0F, 4.0F);
    next.panY = std::clamp(camera.panY + finiteOr(dy, 0.0F), -4.0F, 4.0F);
    return next;
}

Bounds3d boundsOf(const mapping::LedMap& map) noexcept {
    Bounds3d b;
    if (map.world.empty()) {
        return b;
    }
    b.min = b.max = map.world.front();
    for (const Vec3& p : map.world) {
        b.min = {std::min(b.min.x, p.x), std::min(b.min.y, p.y), std::min(b.min.z, p.z)};
        b.max = {std::max(b.max.x, p.x), std::max(b.max.y, p.y), std::max(b.max.z, p.z)};
    }
    b.center = {(b.min.x + b.max.x) * 0.5F, (b.min.y + b.max.y) * 0.5F, (b.min.z + b.max.z) * 0.5F};
    const float r = length(sub(b.max, b.center));
    b.radius = r > 1e-6F ? r : 1.0F;
    return b;
}

Projector::Projector(const Camera3d& camera, const Bounds3d& bounds, std::uint32_t width, std::uint32_t height,
                     std::uint32_t margin)
    : center_{bounds.center},
      radius_{bounds.radius > 1e-6F && std::isfinite(bounds.radius) ? bounds.radius : 1.0F},
      perspective_{camera.perspective} {
    const float yaw = radians(finiteOr(camera.yawDegrees, 0.0F));
    const float pitch = radians(std::clamp(finiteOr(camera.pitchDegrees, 0.0F), -90.0F, 90.0F));
    // Vom Mittelpunkt zur Kamera, rechts und oben auf dem Bildschirm. "Rechts" ergibt sich direkt
    // aus dem Yaw (ohne Kreuzprodukt mit "oben"), darum bleibt die Draufsicht (pitch 90) definiert.
    toCamera_ = {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
    right_ = {std::cos(yaw), 0.0F, -std::sin(yaw)};
    up_ = {toCamera_.y * right_.z - toCamera_.z * right_.y, toCamera_.z * right_.x - toCamera_.x * right_.z,
           toCamera_.x * right_.y - toCamera_.y * right_.x};

    const float side = static_cast<float>(std::min(width, height)) - 2.0F * static_cast<float>(margin);
    const float half = std::max(side, 1.0F) * 0.5F;
    const float zoom = std::clamp(finiteOr(camera.zoom, 1.0F), kMinZoom3d, kMaxZoom3d);
    distance_ = radius_ / std::sin(kHalfFov);
    if (perspective_) {
        scale_ = half / std::tan(kHalfFov) / kFit * zoom;
    } else {
        // Größte Ausdehnung der Box entlang der Bildachsen: die Box füllt das Bild.
        float extentX = 0.0F;
        float extentY = 0.0F;
        for (std::size_t i = 0; i < 8; ++i) {
            const Vec3 corner{(i & 1U) != 0 ? bounds.max.x : bounds.min.x, (i & 2U) != 0 ? bounds.max.y : bounds.min.y,
                              (i & 4U) != 0 ? bounds.max.z : bounds.min.z};
            const Vec3 d = sub(corner, center_);
            extentX = std::max(extentX, std::abs(dot(d, right_)));
            extentY = std::max(extentY, std::abs(dot(d, up_)));
        }
        const float floorExtent = 0.05F * radius_;  // flache Karten: keine Teilung durch 0
        const float availW = std::max(static_cast<float>(width) - 2.0F * static_cast<float>(margin), 1.0F) * 0.5F;
        const float availH = std::max(static_cast<float>(height) - 2.0F * static_cast<float>(margin), 1.0F) * 0.5F;
        scale_ = std::min(availW / std::max(extentX, floorExtent), availH / std::max(extentY, floorExtent)) / kFit * zoom;
    }
    const float shortSide = static_cast<float>(std::min(width, height));
    cx_ = static_cast<float>(width) * 0.5F + finiteOr(camera.panX, 0.0F) * shortSide;
    cy_ = static_cast<float>(height) * 0.5F - finiteOr(camera.panY, 0.0F) * shortSide;
}

std::optional<ScreenPoint> Projector::project(const Vec3& world) const noexcept {
    const Vec3 d = sub(world, center_);
    const float xc = dot(d, right_);
    const float yc = dot(d, up_);
    const float depth = distance_ - dot(d, toCamera_);  // Abstand vor der Kamera
    if (!(depth > 0.02F * distance_)) {
        return std::nullopt;
    }
    ScreenPoint p;
    p.depth = depth;
    if (perspective_) {
        p.pixelsPerUnit = scale_ / depth;
    } else {
        p.pixelsPerUnit = scale_;
    }
    p.x = cx_ + xc * p.pixelsPerUnit;
    p.y = cy_ - yc * p.pixelsPerUnit;
    return p;
}

namespace {

/// Kante des Rahmens als feine Linie (Alpha-Mischung) ins Bild.
void drawEdge(Image& image, const ScreenPoint& a, const ScreenPoint& b) {
    constexpr std::array<float, 3> kLine{74.0F, 84.0F, 110.0F};
    constexpr float kStrength = 0.55F;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::max(std::abs(dx), std::abs(dy));
    if (!std::isfinite(len)) {
        return;
    }
    const int steps = static_cast<int>(std::min(len, 4.0F * static_cast<float>(std::max(image.width, image.height)))) + 1;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(std::lround(a.x + dx * t));
        const int y = static_cast<int>(std::lround(a.y + dy * t));
        if (x < 0 || y < 0 || x >= static_cast<int>(image.width) || y >= static_cast<int>(image.height)) {
            continue;
        }
        std::uint8_t* px = &image.rgba[(static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4];
        for (std::size_t ch = 0; ch < 3; ++ch) {
            px[ch] = static_cast<std::uint8_t>(std::lround(detail::blend(static_cast<float>(px[ch]), kLine[ch], kStrength)));
        }
    }
}

void drawBox(Image& image, const Projector& projector, const Bounds3d& b) {
    std::array<std::optional<ScreenPoint>, 8> corners;
    for (std::size_t i = 0; i < 8; ++i) {
        corners[i] = projector.project(Vec3{(i & 1U) != 0 ? b.max.x : b.min.x, (i & 2U) != 0 ? b.max.y : b.min.y,
                                            (i & 4U) != 0 ? b.max.z : b.min.z});
    }
    for (std::size_t i = 0; i < 8; ++i) {
        for (std::size_t bit = 1; bit < 8; bit <<= 1U) {
            const std::size_t j = i | bit;
            if (j != i && corners[i] && corners[j]) {
                drawEdge(image, *corners[i], *corners[j]);
            }
        }
    }
}

}  // namespace

Result<Image> renderPreview3d(const mapping::LedMap& map, std::span<const ColorRgba> colors, const Camera3d& camera,
                              const PreviewOptions& options) {
    if (map.world.empty()) {
        return Error{ErrorCode::InvalidArgument, "Die LED-Karte ist leer"};
    }
    if (colors.size() != map.world.size()) {
        return Error{ErrorCode::InvalidArgument, "Die Zahl der Farben passt nicht zur LED-Karte"};
    }
    if (options.width == 0 || options.height == 0 || options.width > detail::kMaxSide ||
        options.height > detail::kMaxSide || options.margin * 2 + 2 > std::min(options.width, options.height)) {
        return Error{ErrorCode::InvalidArgument, "Ungültige Bildgröße oder zu großer Rand"};
    }

    const Bounds3d bounds = boundsOf(map);
    const Projector projector{camera, bounds, options.width, options.height, options.margin};

    // Abstand der LEDs je Bereich; ohne Angabe aus der Dichte der Kugel geschätzt.
    const float count = static_cast<float>(map.world.size());
    const float fallback = 2.0F * bounds.radius / std::max(1.0F, std::cbrt(count));
    std::vector<float> ledSpacing(map.world.size(), fallback);
    for (std::size_t r = 0; r < map.ranges.size(); ++r) {
        const mapping::FixtureRange& range = map.ranges[r];
        if (static_cast<std::size_t>(range.first) + range.count > map.world.size()) {
            continue;
        }
        std::fill_n(ledSpacing.begin() + range.first, range.count, rangeSpacing(map, r, fallback));
    }

    // Gamma je LED (nach Fixture), jeder Wert nur einmal als Tabelle (wie in renderPreview).
    std::vector<detail::GammaTable> tables;
    tables.emplace_back(detail::validGamma(options.gamma));
    std::vector<std::uint16_t> ledTable(map.world.size(), 0);
    for (std::size_t r = 0; r < map.ranges.size() && r < options.fixtureGamma.size(); ++r) {
        const mapping::FixtureRange& range = map.ranges[r];
        if (static_cast<std::size_t>(range.first) + range.count > map.world.size()) {
            continue;
        }
        const float gamma = detail::validGamma(options.fixtureGamma[r]);
        auto found = std::find_if(tables.begin(), tables.end(), [gamma](const detail::GammaTable& t) { return t.gamma() == gamma; });
        if (found == tables.end()) {
            tables.emplace_back(gamma);
            found = tables.end() - 1;
        }
        std::fill_n(ledTable.begin() + range.first, range.count, static_cast<std::uint16_t>(found - tables.begin()));
    }

    Image image;
    detail::clearImage(image, options.width, options.height);
    if (camera.showBox) {
        drawBox(image, projector, bounds);
    }

    struct Placed {
        std::uint32_t index{0};
        ScreenPoint point;
    };
    std::vector<Placed> placed;
    placed.reserve(map.world.size());
    for (std::size_t i = 0; i < map.world.size(); ++i) {
        if (const std::optional<ScreenPoint> p = projector.project(map.world[i])) {
            placed.push_back({static_cast<std::uint32_t>(i), *p});
        }
    }
    // Hinten zuerst; bei gleicher Tiefe bleibt die Kabelreihenfolge (stabil, kein Flackern).
    std::stable_sort(placed.begin(), placed.end(), [](const Placed& a, const Placed& b) { return a.point.depth > b.point.depth; });

    const float cue = std::clamp(finiteOr(camera.depthCue, 0.0F), 0.0F, 1.0F);
    const float nearD = projector.nearDepth();
    const float span = std::max(1e-6F, projector.farDepth() - nearD);
    for (const Placed& item : placed) {
        const ColorRgba& c = colors[item.index];
        const float alpha = std::isfinite(c.a) ? std::clamp(c.a, 0.0F, 1.0F) : 1.0F;
        const detail::GammaTable& table = tables[ledTable[item.index]];
        const std::array<float, 3> lit{static_cast<float>(table.byte(c.r * alpha)),
                                       static_cast<float>(table.byte(c.g * alpha)),
                                       static_cast<float>(table.byte(c.b * alpha))};
        const float far = std::clamp((item.point.depth - nearD) / span, 0.0F, 1.0F);
        const float radius = std::clamp(options.dotScale * ledSpacing[item.index] * item.point.pixelsPerUnit, 1.0F, 24.0F);
        detail::drawDisc(image, item.point.x, item.point.y, radius, lit, 1.0F - cue * far);
    }
    return image;
}

}  // namespace whitehole::preview
