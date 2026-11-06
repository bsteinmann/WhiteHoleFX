// fixture_pose.cpp: Lage und Drehung eines Fixtures im Raum (siehe fixture.hpp, worldPositions).
#include <algorithm>
#include <array>
#include <cmath>

#include "whitehole/mapping/fixture.hpp"

namespace whitehole::mapping {

namespace {

constexpr float kDegrees = 3.14159265358979F / 180.0F;

bool isRotated(const Vec3& degrees) noexcept { return degrees.x != 0.0F || degrees.y != 0.0F || degrees.z != 0.0F; }

Vec3 plus(const Vec3& a, const Vec3& b) noexcept { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }

Vec3 center(const Box& box) noexcept {
    return Vec3{(box.min.x + box.max.x) * 0.5F, (box.min.y + box.max.y) * 0.5F, (box.min.z + box.max.z) * 0.5F};
}

}  // namespace

Vec3 rotateAround(const Vec3& point, const Vec3& c, const Vec3& degrees) noexcept {
    float x = point.x - c.x;
    float y = point.y - c.y;
    float z = point.z - c.z;
    if (degrees.x != 0.0F) {  // um x
        const float s = std::sin(degrees.x * kDegrees);
        const float k = std::cos(degrees.x * kDegrees);
        const float ny = y * k - z * s;
        z = y * s + z * k;
        y = ny;
    }
    if (degrees.y != 0.0F) {  // um y
        const float s = std::sin(degrees.y * kDegrees);
        const float k = std::cos(degrees.y * kDegrees);
        const float nx = x * k + z * s;
        z = -x * s + z * k;
        x = nx;
    }
    if (degrees.z != 0.0F) {  // um z
        const float s = std::sin(degrees.z * kDegrees);
        const float k = std::cos(degrees.z * kDegrees);
        const float nx = x * k - y * s;
        y = x * s + y * k;
        x = nx;
    }
    return Vec3{c.x + x, c.y + y, c.z + z};
}

std::vector<Vec3> worldPositions(const Fixture& fixture) {
    std::vector<Vec3> positions = localPositions(fixture.shape);
    const bool rotated = isRotated(fixture.rotation);
    const Vec3 c = center(localBox(fixture.shape));
    for (Vec3& p : positions) {
        if (rotated) {
            p = rotateAround(p, c, fixture.rotation);
        }
        p = plus(p, fixture.origin);
    }
    return positions;
}

Box worldBox(const Fixture& fixture) noexcept {
    const Box local = localBox(fixture.shape);
    if (!isRotated(fixture.rotation)) {
        return Box{plus(local.min, fixture.origin), plus(local.max, fixture.origin)};
    }
    const Vec3 c = center(local);
    Box box{};
    bool first = true;
    for (int i = 0; i < 8; ++i) {
        const Vec3 corner{(i & 1) != 0 ? local.max.x : local.min.x, (i & 2) != 0 ? local.max.y : local.min.y,
                          (i & 4) != 0 ? local.max.z : local.min.z};
        const Vec3 p = plus(rotateAround(corner, c, fixture.rotation), fixture.origin);
        if (first) {
            box = Box{p, p};
            first = false;
        } else {
            box.min = Vec3{std::min(box.min.x, p.x), std::min(box.min.y, p.y), std::min(box.min.z, p.z)};
            box.max = Vec3{std::max(box.max.x, p.x), std::max(box.max.y, p.y), std::max(box.max.z, p.z)};
        }
    }
    return box;
}

}  // namespace whitehole::mapping
