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

bool insideRange(float value, float low, float high) noexcept {
    const float slack = 1e-4F * std::max(high - low, 1e-3F);  // Rundung an der Kante verzeihen
    return value >= low - slack && value <= high + slack;
}

}  // namespace

Result<void> validateRegion(const Region& region) {
    const bool finite = std::isfinite(region.x) && std::isfinite(region.y) && std::isfinite(region.w) &&
                        std::isfinite(region.h);
    if (!finite) {
        return Error{ErrorCode::InvalidArgument, "Der Effektbereich muss aus endlichen Zahlen bestehen"};
    }
    if (!(region.w > 0.0F) || !(region.h > 0.0F)) {
        return Error{ErrorCode::InvalidArgument, "Der Effektbereich braucht Breite und Höhe größer 0"};
    }
    constexpr float kSlack = 1e-6F;
    if (region.x < 0.0F || region.y < 0.0F || region.x + region.w > 1.0F + kSlack ||
        region.y + region.h > 1.0F + kSlack) {
        return Error{ErrorCode::InvalidArgument, "Der Effektbereich muss innerhalb der Wand liegen (0 bis 1)"};
    }
    return {};
}

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

Result<LedMap> Layout::bake(NormalizeMode mode, const std::optional<Region>& region) const {
    if (region) {
        if (Result<void> valid = validateRegion(*region); !valid.ok()) {
            return valid.error();
        }
    }
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

    // Die Box, auf die normalisiert wird: die ganze Wand oder der Effektbereich (z bleibt gleich).
    Box box = map.volume;
    if (region) {
        const float wallWidth = map.volume.max.x - map.volume.min.x;
        const float wallHeight = map.volume.max.y - map.volume.min.y;
        box.min.x = map.volume.min.x + region->x * wallWidth;
        box.max.x = box.min.x + region->w * wallWidth;
        box.max.y = map.volume.max.y - region->y * wallHeight;  // y zählt von oben
        box.min.y = box.max.y - region->h * wallHeight;
    }
    const Vec3 extent{box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z};
    const float longest = std::max({extent.x, extent.y, extent.z});

    map.unit.reserve(map.world.size());
    for (const Vec3& p : map.world) {
        const float sx = mode == NormalizeMode::Stretch ? extent.x : longest;
        const float sy = mode == NormalizeMode::Stretch ? extent.y : longest;
        const float sz = mode == NormalizeMode::Stretch ? extent.z : longest;
        map.unit.push_back(Vec3{normalizeAxis(p.x, box.min.x, extent.x, sx), normalizeAxis(p.y, box.min.y, extent.y, sy),
                                normalizeAxis(p.z, box.min.z, extent.z, sz)});
        if (region) {
            const bool inside = insideRange(p.x, box.min.x, box.max.x) && insideRange(p.y, box.min.y, box.max.y);
            map.outside.push_back(inside ? 0U : 1U);
        }
    }
    return map;
}

}  // namespace whitehole::mapping
