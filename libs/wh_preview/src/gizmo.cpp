// gizmo.cpp: Umsetzung zu gizmo.hpp.
#include "whitehole/preview/gizmo.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::preview {

namespace {

constexpr float kPi = 3.14159265358979F;

Vec3 axisVector(int axis) noexcept {
    return Vec3{axis == 0 ? 1.0F : 0.0F, axis == 1 ? 1.0F : 0.0F, axis == 2 ? 1.0F : 0.0F};
}


/// Ebenenachsen (u, v) des Rings um `axis`, rechtshändig: u x v = Achse.
void ringPlane(int axis, Vec3& u, Vec3& v) noexcept {
    u = axisVector((axis + 1) % 3);
    v = axisVector((axis + 2) % 3);
}

void fillRings(const Projector& projector, const Vec3& anchor, GizmoGeometry& gizmo) {
    const float radius = gizmo.worldLength * kGizmoRingRadius;
    for (int axis = 0; axis < 3; ++axis) {
        const auto a = static_cast<std::size_t>(axis);
        Vec3 u;
        Vec3 v;
        ringPlane(axis, u, v);
        gizmo.ringVisible[a] = true;
        for (int i = 0; i <= kGizmoRingSegments; ++i) {
            const float t = 2.0F * kPi * static_cast<float>(i % kGizmoRingSegments) / static_cast<float>(kGizmoRingSegments);
            const float c = std::cos(t) * radius;
            const float s = std::sin(t) * radius;
            const std::optional<ScreenPoint> p = projector.project(
                Vec3{anchor.x + u.x * c + v.x * s, anchor.y + u.y * c + v.y * s, anchor.z + u.z * c + v.z * s});
            if (!p) {
                gizmo.ringVisible[a] = false;
                break;
            }
            gizmo.ringX[a][static_cast<std::size_t>(i)] = p->x;
            gizmo.ringY[a][static_cast<std::size_t>(i)] = p->y;
        }
    }
}

/// Nächster Punkt auf der Strecke (ax, ay)-(bx, by) zu (x, y): Abstand und Anteil 0..1 entlang der Strecke.
void nearestOnSegment(float ax, float ay, float bx, float by, float x, float y, float& distance, float& fraction) noexcept {
    const float dx = bx - ax;
    const float dy = by - ay;
    const float lengthSquared = dx * dx + dy * dy;
    fraction = lengthSquared > 0.0F ? std::clamp(((x - ax) * dx + (y - ay) * dy) / lengthSquared, 0.0F, 1.0F) : 0.0F;
    distance = std::hypot(x - (ax + dx * fraction), y - (ay + dy * fraction));
}

}  // namespace

std::optional<GizmoGeometry> gizmoFor(const Projector& projector, const Vec3& anchor, float lengthPixels) {
    const std::optional<ScreenPoint> origin = projector.project(anchor);
    if (!origin || origin->pixelsPerUnit <= 0.0F || lengthPixels <= 0.0F) {
        return std::nullopt;
    }
    // Ist das Bild sehr klein (oder das Fixture nah an der Kamera), reicht ein Pfeil in Wunschlänge bis hinter die
    // Kamera: dann Schritt für Schritt kürzer versuchen.
    for (int attempt = 0; attempt < 8; ++attempt, lengthPixels *= 0.5F) {
        GizmoGeometry gizmo;
        gizmo.originX = origin->x;
        gizmo.originY = origin->y;
        gizmo.worldLength = lengthPixels / origin->pixelsPerUnit;
        bool visible = true;
        for (int axis = 0; axis < 3 && visible; ++axis) {
            const Vec3 direction = axisVector(axis);
            const Vec3 tip{anchor.x + direction.x * gizmo.worldLength, anchor.y + direction.y * gizmo.worldLength,
                           anchor.z + direction.z * gizmo.worldLength};
            const std::optional<ScreenPoint> projected = projector.project(tip);
            if (!projected) {
                visible = false;
                break;
            }
            gizmo.tipX[static_cast<std::size_t>(axis)] = projected->x;
            gizmo.tipY[static_cast<std::size_t>(axis)] = projected->y;
        }
        if (visible) {
            fillRings(projector, anchor, gizmo);
            return gizmo;
        }
    }
    return std::nullopt;
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

float gizmoRingMinorPixels(const GizmoGeometry& gizmo, int axis) noexcept {
    const auto a = static_cast<std::size_t>(std::clamp(axis, 0, 2));
    if (!gizmo.ringVisible[a]) {
        return 0.0F;
    }
    // Ellipse: Fläche = pi * große * kleine Halbachse. Fläche aus dem Vieleck (Gauß'sche Trapezformel), große Halbachse
    // = größter Abstand vom Mittelpunkt (Mittel der Ecken).
    float cx = 0.0F;
    float cy = 0.0F;
    float twiceArea = 0.0F;
    for (int i = 0; i < kGizmoRingSegments; ++i) {
        const auto k = static_cast<std::size_t>(i);
        cx += gizmo.ringX[a][k];
        cy += gizmo.ringY[a][k];
        twiceArea += gizmo.ringX[a][k] * gizmo.ringY[a][k + 1] - gizmo.ringX[a][k + 1] * gizmo.ringY[a][k];
    }
    cx /= static_cast<float>(kGizmoRingSegments);
    cy /= static_cast<float>(kGizmoRingSegments);
    float major = 0.0F;
    for (int i = 0; i < kGizmoRingSegments; ++i) {
        const auto k = static_cast<std::size_t>(i);
        major = std::max(major, std::hypot(gizmo.ringX[a][k] - cx, gizmo.ringY[a][k] - cy));
    }
    return major > 0.0F ? std::abs(twiceArea) * 0.5F / (kPi * major) : 0.0F;
}

std::optional<int> pickGizmoRing(const GizmoGeometry& gizmo, float x, float y, float tolerance) noexcept {
    std::optional<int> best;
    float bestDistance = tolerance;
    for (int axis = 0; axis < 3; ++axis) {
        if (gizmoRingMinorPixels(gizmo, axis) < kGizmoRingMinMinorPixels) {
            continue;
        }
        const auto a = static_cast<std::size_t>(axis);
        for (int i = 0; i < kGizmoRingSegments; ++i) {
            const auto k = static_cast<std::size_t>(i);
            float distance = 0.0F;
            float fraction = 0.0F;
            nearestOnSegment(gizmo.ringX[a][k], gizmo.ringY[a][k], gizmo.ringX[a][k + 1], gizmo.ringY[a][k + 1], x, y, distance, fraction);
            if (distance <= bestDistance) {
                bestDistance = distance;
                best = axis;
            }
        }
    }
    return best;
}

float gizmoRingAngle(const GizmoGeometry& gizmo, int axis, float x, float y) noexcept {
    const auto a = static_cast<std::size_t>(std::clamp(axis, 0, 2));
    float bestDistance = 1e30F;
    float bestIndex = 0.0F;
    for (int i = 0; i < kGizmoRingSegments; ++i) {
        const auto k = static_cast<std::size_t>(i);
        float distance = 0.0F;
        float fraction = 0.0F;
        nearestOnSegment(gizmo.ringX[a][k], gizmo.ringY[a][k], gizmo.ringX[a][k + 1], gizmo.ringY[a][k + 1], x, y, distance, fraction);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<float>(i) + fraction;
        }
    }
    return std::fmod(360.0F * bestIndex / static_cast<float>(kGizmoRingSegments), 360.0F);
}

float angleDelta(float from, float to) noexcept {
    float d = std::fmod(to - from, 360.0F);
    if (d > 180.0F) {
        d -= 360.0F;
    } else if (d <= -180.0F) {
        d += 360.0F;
    }
    return d;
}

}  // namespace whitehole::preview
