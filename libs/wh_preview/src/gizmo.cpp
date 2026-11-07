// gizmo.cpp: Umsetzung zu gizmo.hpp.
#include "whitehole/preview/gizmo.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::preview {

namespace {

Vec3 axisVector(int axis) noexcept {
    return Vec3{axis == 0 ? 1.0F : 0.0F, axis == 1 ? 1.0F : 0.0F, axis == 2 ? 1.0F : 0.0F};
}

}  // namespace

std::optional<GizmoGeometry> gizmoFor(const Projector& projector, const Vec3& anchor, float lengthPixels) {
    const std::optional<ScreenPoint> origin = projector.project(anchor);
    if (!origin || origin->pixelsPerUnit <= 0.0F || lengthPixels <= 0.0F) {
        return std::nullopt;
    }
    GizmoGeometry gizmo;
    gizmo.originX = origin->x;
    gizmo.originY = origin->y;
    gizmo.worldLength = lengthPixels / origin->pixelsPerUnit;
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 direction = axisVector(axis);
        const Vec3 tip{anchor.x + direction.x * gizmo.worldLength, anchor.y + direction.y * gizmo.worldLength,
                       anchor.z + direction.z * gizmo.worldLength};
        const std::optional<ScreenPoint> projected = projector.project(tip);
        if (!projected) {
            return std::nullopt;
        }
        gizmo.tipX[static_cast<std::size_t>(axis)] = projected->x;
        gizmo.tipY[static_cast<std::size_t>(axis)] = projected->y;
    }
    return gizmo;
}

float gizmoAxisPixels(const GizmoGeometry& gizmo, int axis) noexcept {
    const auto i = static_cast<std::size_t>(std::clamp(axis, 0, 2));
    return std::hypot(gizmo.tipX[i] - gizmo.originX, gizmo.tipY[i] - gizmo.originY);
}

std::optional<int> pickGizmoAxis(const GizmoGeometry& gizmo, float x, float y, float tolerance) noexcept {
    std::optional<int> best;
    float bestDistance = tolerance;
    for (int axis = 0; axis < 3; ++axis) {
        if (gizmoAxisPixels(gizmo, axis) < kGizmoMinPixels) {
            continue;
        }
        const auto i = static_cast<std::size_t>(axis);
        const float vx = gizmo.tipX[i] - gizmo.originX;
        const float vy = gizmo.tipY[i] - gizmo.originY;
        const float lengthSquared = vx * vx + vy * vy;
        const float t = std::clamp(((x - gizmo.originX) * vx + (y - gizmo.originY) * vy) / lengthSquared, 0.0F, 1.0F);
        const float distance = std::hypot(x - (gizmo.originX + t * vx), y - (gizmo.originY + t * vy));
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = axis;
        }
    }
    return best;
}

float gizmoDragDistance(const GizmoGeometry& gizmo, int axis, float dx, float dy) noexcept {
    const float pixels = gizmoAxisPixels(gizmo, axis);
    if (pixels < kGizmoMinPixels) {
        return 0.0F;
    }
    const auto i = static_cast<std::size_t>(axis);
    const float vx = gizmo.tipX[i] - gizmo.originX;
    const float vy = gizmo.tipY[i] - gizmo.originY;
    const float along = (dx * vx + dy * vy) / pixels;  // Bildpunkte in Pfeilrichtung (mit Vorzeichen)
    return along / pixels * gizmo.worldLength;
}

float snapDistance(float value, float step) noexcept {
    if (step <= 0.0F) {
        return value;
    }
    return std::round(value / step) * step;
}

}  // namespace whitehole::preview
