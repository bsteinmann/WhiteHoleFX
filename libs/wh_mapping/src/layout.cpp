#include "whitehole/mapping/layout.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace whitehole::mapping {

namespace {

bool finiteVec(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3 plus(const Vec3& a, const Vec3& b) noexcept { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }

float clamp01(float v) noexcept { return std::clamp(v, 0.0F, 1.0F); }

/// Eine Achse: Wert -> [0,1]. `scale` ist die Länge, die auf 1 abgebildet wird.
float normalizeAxis(float value, float low, float extent, float scale) noexcept {
    if (!(extent > 0.0F)) {
        return 0.0F;  // keine Ausdehnung
    }
    // Zentriert: Die Mitte der Achse landet bei 0,5. Bei Stretch ist scale == extent,
    // dann ist das gleichbedeutend mit (value - low) / extent.
    const float center = low + extent * 0.5F;
    return clamp01(0.5F + (value - center) / scale);
}

}  // namespace

Result<FixtureId> Layout::add(std::string name, const Vec3& origin, FixtureShape shape) {
    if (!finiteVec(origin)) {
        return Error{ErrorCode::InvalidArgument, "Der Ursprung des Fixtures muss endlich sein"};
    }
    if (Result<void> valid = validateShape(shape); !valid.ok()) {
        return valid.error();
    }
    const FixtureId id = ids_.next();
    fixtures_.push_back(Fixture{id, std::move(name), origin, std::move(shape)});
    return id;
}

Result<void> Layout::remove(FixtureId id) {
    const auto it = std::find_if(fixtures_.begin(), fixtures_.end(),
                                 [id](const Fixture& f) { return f.id == id; });
    if (it == fixtures_.end()) {
        return Error{ErrorCode::NotFound, "Fixture unbekannt"};
    }
    fixtures_.erase(it);
    return {};
}

Result<void> Layout::move(FixtureId id, const Vec3& origin) {
    if (!finiteVec(origin)) {
        return Error{ErrorCode::InvalidArgument, "Der Ursprung des Fixtures muss endlich sein"};
    }
    for (Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            fixture.origin = origin;
            return {};
        }
    }
    return Error{ErrorCode::NotFound, "Fixture unbekannt"};
}

Result<void> Layout::rename(FixtureId id, std::string name) {
    for (Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            fixture.name = std::move(name);
            return {};
        }
    }
    return Error{ErrorCode::NotFound, "Fixture unbekannt"};
}

Result<void> Layout::replaceShape(FixtureId id, FixtureShape shape) {
    for (Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            if (Result<void> valid = validateShape(shape); !valid.ok()) {
                return valid;
            }
            fixture.shape = std::move(shape);
            return {};
        }
    }
    return Error{ErrorCode::NotFound, "Fixture unbekannt"};
}

const Fixture* Layout::find(FixtureId id) const noexcept {
    for (const Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            return &fixture;
        }
    }
    return nullptr;
}

Result<LedMap> Layout::bake(NormalizeMode mode) const {
    if (fixtures_.empty()) {
        return Error{ErrorCode::InvalidArgument, "Das Layout enthält keine Fixtures"};
    }
    std::uint64_t total = 0;
    for (const Fixture& fixture : fixtures_) {
        total += ledCount(fixture.shape);
        if (total > kMaxLedsPerLayout) {
            return Error{ErrorCode::InvalidArgument, "Das Layout hat zu viele LEDs"};
        }
    }

    LedMap map;
    map.world.reserve(static_cast<std::size_t>(total));

    bool first = true;
    for (const Fixture& fixture : fixtures_) {
        const Box local = localBox(fixture.shape);
        const Box box{plus(local.min, fixture.origin), plus(local.max, fixture.origin)};
        if (first) {
            map.volume = box;
            first = false;
        } else {
            map.volume.min = Vec3{std::min(map.volume.min.x, box.min.x), std::min(map.volume.min.y, box.min.y),
                                  std::min(map.volume.min.z, box.min.z)};
            map.volume.max = Vec3{std::max(map.volume.max.x, box.max.x), std::max(map.volume.max.y, box.max.y),
                                  std::max(map.volume.max.z, box.max.z)};
        }

        const auto start = static_cast<std::uint32_t>(map.world.size());
        for (const Vec3& p : localPositions(fixture.shape)) {
            map.world.push_back(plus(p, fixture.origin));
        }
        map.ranges.push_back(
            FixtureRange{fixture.id, start, static_cast<std::uint32_t>(map.world.size()) - start});
    }

    const Vec3 extent{map.volume.max.x - map.volume.min.x, map.volume.max.y - map.volume.min.y,
                      map.volume.max.z - map.volume.min.z};
    const float longest = std::max({extent.x, extent.y, extent.z});

    map.unit.reserve(map.world.size());
    for (const Vec3& p : map.world) {
        const float sx = mode == NormalizeMode::Stretch ? extent.x : longest;
        const float sy = mode == NormalizeMode::Stretch ? extent.y : longest;
        const float sz = mode == NormalizeMode::Stretch ? extent.z : longest;
        map.unit.push_back(Vec3{normalizeAxis(p.x, map.volume.min.x, extent.x, sx),
                                normalizeAxis(p.y, map.volume.min.y, extent.y, sy),
                                normalizeAxis(p.z, map.volume.min.z, extent.z, sz)});
    }
    return map;
}

}  // namespace whitehole::mapping
