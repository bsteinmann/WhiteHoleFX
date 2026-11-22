// layout.cpp: Umsetzung zu layout.hpp. Alle Fixtures eines Projekts und ihre Abbildung auf den Einheitswürfel.
#include "whitehole/mapping/layout.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "whitehole/mapping/projection.hpp"

namespace whitehole::mapping {

namespace {

bool finiteVec(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}


Box unite(const Box& a, const Box& b) noexcept {
    return Box{Vec3{std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z)},
               Vec3{std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z)}};
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
    fixtures_.push_back(Fixture{id, std::move(name), origin, std::move(shape), Vec3{}, std::string{}, Projection::Volume});
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

Result<void> Layout::rotate(FixtureId id, const Vec3& degrees) {
    if (!finiteVec(degrees)) {
        return Error{ErrorCode::InvalidArgument, "Die Drehung des Fixtures muss aus endlichen Zahlen bestehen"};
    }
    for (Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            fixture.rotation = degrees;
            return {};
        }
    }
    return Error{ErrorCode::NotFound, "Fixture unbekannt"};
}

Result<void> Layout::setType(FixtureId id, std::string type) {
    for (Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            fixture.type = std::move(type);
            return {};
        }
    }
    return Error{ErrorCode::NotFound, "Fixture unbekannt"};
}

Result<void> Layout::setProjection(FixtureId id, Projection projection) {
    for (Fixture& fixture : fixtures_) {
        if (fixture.id == id) {
            fixture.projection = projection;
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
        const Box box = worldBox(fixture);
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
        for (const Vec3& p : worldPositions(fixture)) {
            map.world.push_back(p);
        }
        map.ranges.push_back(
            FixtureRange{fixture.id, start, static_cast<std::uint32_t>(map.world.size()) - start});
        map.spacing.push_back(ledSpacing(fixture.shape));
    }

    // Bezugsboxen: Volume-Fixtures normalisieren sich auf ihre eigene Box, Box-Fixtures werden auf die Box aller Box-Fixtures
    // projiziert. Ohne projizierte Fixtures ist die Volume-Box genau map.volume (wie vor der Projektion).
    Box volumeBox{};
    Box boxBox{};
    bool anyVolume = false;
    bool anyBox = false;
    for (const Fixture& fixture : fixtures_) {
        if (fixture.projection == Projection::Faces) {
            continue;
        }
        const Box box = worldBox(fixture);
        Box& target = fixture.projection == Projection::Volume ? volumeBox : boxBox;
        bool& any = fixture.projection == Projection::Volume ? anyVolume : anyBox;
        target = any ? unite(target, box) : box;
        any = true;
    }

    // Bezugsgröße für @aspect/@columns/@rows: Volume vor Box vor Faces (siehe LedMap::frameBox).
    const auto finest = [&](Projection projection) {
        float spacing = 0.0F;
        for (std::size_t i = 0; i < fixtures_.size(); ++i) {
            const float s = map.spacing[i];
            if (fixtures_[i].projection == projection && s > 0.0F && (spacing == 0.0F || s < spacing)) {
                spacing = s;
            }
        }
        return spacing;
    };
    if (anyVolume) {
        map.frameBox = volumeBox;
        map.frameSpacing = finest(Projection::Volume);
    } else if (anyBox) {
        map.frameBox = boxBox;
        map.frameSpacing = finest(Projection::Box);
    } else {
        for (const Fixture& fixture : fixtures_) {  // nur Faces-Fixtures: die ungedrehte Box des ersten
            Fixture bare = fixture;
            bare.origin = Vec3{};
            bare.rotation = Vec3{};
            map.frameBox = worldBox(bare);
            break;
        }
        map.frameSpacing = finest(Projection::Faces);
    }

    const Vec3 extent{volumeBox.max.x - volumeBox.min.x, volumeBox.max.y - volumeBox.min.y,
                      volumeBox.max.z - volumeBox.min.z};
    const float longest = std::max({extent.x, extent.y, extent.z});

    map.unit.reserve(map.world.size());
    for (std::size_t i = 0; i < fixtures_.size(); ++i) {
        const Fixture& fixture = fixtures_[i];
        const FixtureRange& range = map.ranges[i];
        if (fixture.projection == Projection::Volume) {
            for (std::uint32_t k = 0; k < range.count; ++k) {
                const Vec3& p = map.world[range.first + k];
                const float sx = mode == NormalizeMode::Stretch ? extent.x : longest;
                const float sy = mode == NormalizeMode::Stretch ? extent.y : longest;
                const float sz = mode == NormalizeMode::Stretch ? extent.z : longest;
                map.unit.push_back(Vec3{normalizeAxis(p.x, volumeBox.min.x, extent.x, sx),
                                        normalizeAxis(p.y, volumeBox.min.y, extent.y, sy),
                                        normalizeAxis(p.z, volumeBox.min.z, extent.z, sz)});
            }
        } else {
            const std::vector<Vec3> projected =
                fixture.projection == Projection::Faces
                    ? projectFaces(fixture, mode)
                    : projectBox(fixture,
                                 std::vector<Vec3>(map.world.begin() + range.first,
                                                   map.world.begin() + range.first + range.count),
                                 boxBox, mode);
            map.unit.insert(map.unit.end(), projected.begin(), projected.end());
        }
    }
    return map;
}

}  // namespace whitehole::mapping
