// projection.cpp: Umsetzung zu projection.hpp (Faces und Box als 2D-Abbildung eines Fixtures).
#include "whitehole/mapping/projection.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

namespace whitehole::mapping {

namespace {

/// Ebene Fixtures haben eine Flächennormale (lokal +z): Matrix, Ring und ein Gitter mit einer Lage.
bool isPlanar(const FixtureShape& shape) noexcept {
    if (std::holds_alternative<MatrixShape>(shape) || std::holds_alternative<RingShape>(shape)) {
        return true;
    }
    if (const auto* grid = std::get_if<GridShape>(&shape)) {
        return grid->nz == 1;
    }
    return false;
}

/// Achse (0 x, 1 y, 2 z), in die die gedrehte lokale z-Achse am stärksten zeigt; bei Gleichstand die frühere.
int normalAxis(const Vec3& rotationDegrees) noexcept {
    const Vec3 n = rotateAround(Vec3{0.0F, 0.0F, 1.0F}, Vec3{}, rotationDegrees);
    const float ax = std::abs(n.x);
    const float ay = std::abs(n.y);
    const float az = std::abs(n.z);
    if (ax >= ay && ax >= az) {
        return 0;
    }
    return ay >= az ? 1 : 2;
}

/// Fläche einer LED eines nicht ebenen Fixtures: Achse ohne Ausdehnung zuerst, sonst die größte Abweichung von der Mitte
/// (bezogen auf die halbe Ausdehnung), bei Gleichstand die frühere Achse x, y, z.
int faceAxisOf(const Vec3& p, const Box& box) noexcept {
    const Vec3 e{box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z};
    if (!(e.z > 0.0F)) {
        return 2;
    }
    if (!(e.y > 0.0F)) {
        return 1;
    }
    if (!(e.x > 0.0F)) {
        return 0;
    }
    const auto deviation = [](float v, float low, float extent) { return std::abs(v - (low + extent * 0.5F)) / (extent * 0.5F); };
    const float dx = deviation(p.x, box.min.x, e.x);
    const float dy = deviation(p.y, box.min.y, e.y);
    const float dz = deviation(p.z, box.min.z, e.z);
    if (dx >= dy && dx >= dz) {
        return 0;
    }
    return dy >= dz ? 1 : 2;
}

}  // namespace

float normalizeAxis(float value, float low, float extent, float scale) noexcept {
    if (!(extent > 0.0F)) {
        return 0.0F;  // keine Ausdehnung
    }
    // Zentriert: Die Mitte der Achse landet bei 0,5. Bei Stretch ist scale == extent,
    // dann ist das gleichbedeutend mit (value - low) / extent.
    const float center = low + extent * 0.5F;
    return std::clamp(0.5F + (value - center) / scale, 0.0F, 1.0F);
}

std::vector<Vec3> projectFaces(const Fixture& fixture, NormalizeMode mode) {
    const std::vector<Vec3> local = localPositions(fixture.shape);
    Fixture bare = fixture;  // die Box des ungedrehten Fixtures am Ursprung (Zellen, nicht nur LED-Mitten)
    bare.origin = Vec3{};
    bare.rotation = Vec3{};
    const Box box = worldBox(bare);
    const float ex = box.max.x - box.min.x;
    const float ey = box.max.y - box.min.y;
    const float longest = std::max(ex, ey);
    const float sx = mode == NormalizeMode::Stretch ? ex : longest;
    const float sy = mode == NormalizeMode::Stretch ? ey : longest;
    std::vector<Vec3> out;
    out.reserve(local.size());
    for (const Vec3& p : local) {
        out.push_back(Vec3{normalizeAxis(p.x, box.min.x, ex, sx), normalizeAxis(p.y, box.min.y, ey, sy), 0.0F});
    }
    return out;
}

std::vector<Vec3> projectBox(const Fixture& fixture, const std::vector<Vec3>& world, const Box& reference,
                             NormalizeMode mode) {
    const Vec3 e{reference.max.x - reference.min.x, reference.max.y - reference.min.y, reference.max.z - reference.min.z};
    const float longest = std::max({e.x, e.y, e.z});
    const auto scale = [&](float extent) { return mode == NormalizeMode::Stretch ? extent : longest; };
    const bool planar = isPlanar(fixture.shape);
    const int fixedAxis = planar ? normalAxis(fixture.rotation) : -1;

    std::vector<Vec3> out;
    out.reserve(world.size());
    for (const Vec3& p : world) {
        const int axis = planar ? fixedAxis : faceAxisOf(p, reference);
        // (u, v) je Fläche: z -> (x, y), x -> (z, y), y -> (x, z)
        float u = 0.0F;
        float v = 0.0F;
        if (axis == 2) {
            u = normalizeAxis(p.x, reference.min.x, e.x, scale(e.x));
            v = normalizeAxis(p.y, reference.min.y, e.y, scale(e.y));
        } else if (axis == 0) {
            u = normalizeAxis(p.z, reference.min.z, e.z, scale(e.z));
            v = normalizeAxis(p.y, reference.min.y, e.y, scale(e.y));
        } else {
            u = normalizeAxis(p.x, reference.min.x, e.x, scale(e.x));
            v = normalizeAxis(p.z, reference.min.z, e.z, scale(e.z));
        }
        out.push_back(Vec3{u, v, 0.0F});
    }
    return out;
}

}  // namespace whitehole::mapping
