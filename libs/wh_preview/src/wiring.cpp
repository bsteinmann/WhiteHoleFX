// wiring.cpp: Umsetzung zu wiring.hpp.
#include "whitehole/preview/wiring.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::preview {

namespace {

bool wanted(std::span<const mapping::FixtureId> fixtures, mapping::FixtureId id) {
    return fixtures.empty() || std::find(fixtures.begin(), fixtures.end(), id) != fixtures.end();
}

}  // namespace

std::vector<WirePath> projectWiring(const mapping::LedMap& map, const Camera3d& camera, std::uint32_t width,
                                    std::uint32_t height, std::uint32_t margin,
                                    std::span<const mapping::FixtureId> fixtures) {
    std::vector<WirePath> paths;
    if (map.world.empty()) {
        return paths;
    }
    const Projector projector{camera, boundsOf(map), width, height, margin};
    for (std::size_t r = 0; r < map.ranges.size(); ++r) {
        const mapping::FixtureRange& range = map.ranges[r];
        if (!wanted(fixtures, range.fixture) || static_cast<std::size_t>(range.first) + range.count > map.world.size()) {
            continue;
        }
        WirePath path;
        path.range = r;
        for (std::uint32_t i = 0; i < range.count; ++i) {
            const std::uint32_t led = range.first + i;
            if (const auto p = projector.project(map.world[led])) {
                path.dots.push_back({p->x, p->y, led});
            }
        }
        paths.push_back(std::move(path));
    }
    return paths;
}

std::optional<PickedLed> pickLed(const mapping::LedMap& map, const Camera3d& camera, std::uint32_t width,
                                 std::uint32_t height, std::uint32_t margin, float x, float y, float maxDistance,
                                 std::span<const mapping::FixtureId> fixtures) {
    if (map.world.empty()) {
        return std::nullopt;
    }
    const Projector projector{camera, boundsOf(map), width, height, margin};
    std::optional<PickedLed> best;
    float bestDistance = maxDistance;
    float bestDepth = 0.0F;
    for (std::size_t r = 0; r < map.ranges.size(); ++r) {
        const mapping::FixtureRange& range = map.ranges[r];
        if (!wanted(fixtures, range.fixture) || static_cast<std::size_t>(range.first) + range.count > map.world.size()) {
            continue;
        }
        for (std::uint32_t i = 0; i < range.count; ++i) {
            const auto p = projector.project(map.world[range.first + i]);
            if (!p) {
                continue;
            }
            const float d = std::hypot(p->x - x, p->y - y);
            // Gleich nah (innerhalb eines halben Pixels): die vordere LED gewinnt.
            const bool better = !best || d < bestDistance - 0.5F || (d <= bestDistance + 0.5F && p->depth < bestDepth);
            if (d <= maxDistance && better) {
                best = PickedLed{range.first + i, r, i};
                bestDistance = d;
                bestDepth = p->depth;
            }
        }
    }
    return best;
}

}  // namespace whitehole::preview
