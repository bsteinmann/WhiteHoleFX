// output_target.cpp: Umsetzung zu output_target.hpp.
#include "whitehole/project/output_target.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::project {

namespace {

constexpr float kMinHalfCell = 0.01F;  // Mindest-Rand, wenn eine Achse keine Ausdehnung hat (z. B. waagerechter Strip)

struct Bounds {
    float minX{1.0F}, minY{1.0F}, maxX{0.0F}, maxY{0.0F};
    bool any{false};
    void add(float x0, float y0, float x1, float y1) {
        minX = any ? std::min(minX, x0) : x0;
        minY = any ? std::min(minY, y0) : y0;
        maxX = any ? std::max(maxX, x1) : x1;
        maxY = any ? std::max(maxY, y1) : y1;
        any = true;
    }
};

/// Fügt die LEDs eines Fixtures hinzu, die Zellen eingerechnet: halber LED-Abstand, in Einheitskoordinaten.
void addFixture(Bounds& bounds, const mapping::LedMap& map, std::size_t rangeIndex) {
    const mapping::FixtureRange& range = map.ranges[rangeIndex];
    if (range.count == 0 || static_cast<std::size_t>(range.first) + range.count > map.unit.size()) {
        return;
    }
    float ux0 = 1.0F, uy0 = 1.0F, ux1 = 0.0F, uy1 = 0.0F;
    float wx0 = 0.0F, wy0 = 0.0F, wx1 = 0.0F, wy1 = 0.0F;
    for (std::uint32_t i = 0; i < range.count; ++i) {
        const Vec3 u = map.unit[range.first + i];
        const Vec3 w = map.world[range.first + i];
        if (i == 0) {
            ux0 = ux1 = u.x; uy0 = uy1 = u.y; wx0 = wx1 = w.x; wy0 = wy1 = w.y;
            continue;
        }
        ux0 = std::min(ux0, u.x); ux1 = std::max(ux1, u.x);
        uy0 = std::min(uy0, u.y); uy1 = std::max(uy1, u.y);
        wx0 = std::min(wx0, w.x); wx1 = std::max(wx1, w.x);
        wy0 = std::min(wy0, w.y); wy1 = std::max(wy1, w.y);
    }
    const float half = rangeIndex < map.spacing.size() ? map.spacing[rangeIndex] * 0.5F : 0.0F;
    // Umrechnung Welt → Einheit je Achse aus dem Fixture selbst; ohne Ausdehnung gilt der Mindest-Rand.
    const float sx = wx1 - wx0 > 1e-6F ? (ux1 - ux0) / (wx1 - wx0) : 0.0F;
    const float sy = wy1 - wy0 > 1e-6F ? (uy1 - uy0) / (wy1 - wy0) : 0.0F;
    const float hx = std::max(kMinHalfCell, half * sx);
    const float hy = std::max(kMinHalfCell, half * sy);
    bounds.add(ux0 - hx, uy0 - hy, ux1 + hx, uy1 + hy);
}

bool fixtureRange(const Project& project, const mapping::LedMap& map, const std::string& name, std::size_t& index) {
    for (const mapping::Fixture& fixture : project.layout.fixtures()) {
        if (fixture.name != name) {
            continue;
        }
        for (std::size_t r = 0; r < map.ranges.size(); ++r) {
            if (map.ranges[r].fixture == fixture.id) {
                index = r;
                return true;
            }
        }
    }
    return false;
}

}  // namespace

Result<TargetResolution> resolveTargets(const Project& project, const mapping::LedMap& map,
                                        const std::vector<std::string>& targets) {
    TargetResolution result;
    Bounds bounds;
    for (const std::string& entry : targets) {
        const std::size_t colon = entry.find(':');
        const std::string kind = colon == std::string::npos ? std::string{} : entry.substr(0, colon);
        const std::string name = colon == std::string::npos ? entry : entry.substr(colon + 1);
        bool found = false;
        std::size_t index = 0;
        if (kind == "fixture") {
            if (fixtureRange(project, map, name, index)) {
                addFixture(bounds, map, index);
                found = true;
            }
        } else if (kind == "group") {
            for (const FixtureGroup& group : project.groups) {
                if (group.name != name) {
                    continue;
                }
                found = true;
                for (const std::string& member : group.fixtures) {
                    if (fixtureRange(project, map, member, index)) {
                        addFixture(bounds, map, index);
                    }
                }
            }
        } else if (kind == "region") {
            for (const StageRegion& region : project.regions) {
                if (region.name == name) {
                    // `top` zählt von oben, die Einheitskoordinate y von unten.
                    bounds.add(region.left, 1.0F - region.top - region.height, region.left + region.width, 1.0F - region.top);
                    found = true;
                }
            }
        }
        if (!found) {
            result.unknown.push_back(entry);
        }
    }
    if (!bounds.any) {
        return Error{ErrorCode::InvalidArgument, "Das Ziel trifft kein Fixture, keine Gruppe und keine Region"};
    }
    const float left = std::clamp(bounds.minX, 0.0F, 1.0F);
    const float right = std::clamp(bounds.maxX, 0.0F, 1.0F);
    const float bottom = std::clamp(bounds.minY, 0.0F, 1.0F);
    const float topEdge = std::clamp(bounds.maxY, 0.0F, 1.0F);
    result.rect = TargetRect{left, 1.0F - topEdge, std::max(right - left, 0.001F), std::max(topEdge - bottom, 0.001F)};
    return result;
}

namespace {

void applyGraph(graph::Graph& graph, const Project& project, const mapping::LedMap& map, const std::string& where,
                std::vector<std::string>& unknown) {
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (node.targets.empty() || node.typeId != "wh.output.canvas") {
            continue;
        }
        const Result<TargetResolution> resolved = resolveTargets(project, map, node.targets);
        if (!resolved.ok()) {
            for (const std::string& entry : node.targets) {
                unknown.push_back(where + ": " + entry);
            }
            continue;
        }
        for (const std::string& entry : resolved.value().unknown) {
            unknown.push_back(where + ": " + entry);
        }
        const TargetRect& rect = resolved.value().rect;
        (void)graph.setParameter(node.id, "left", static_cast<double>(rect.left));
        (void)graph.setParameter(node.id, "top", static_cast<double>(rect.top));
        (void)graph.setParameter(node.id, "width", static_cast<double>(rect.width));
        (void)graph.setParameter(node.id, "height", static_cast<double>(rect.height));
    }
}

}  // namespace

std::vector<std::string> applyOutputTargets(Project& project, const mapping::LedMap& map) {
    std::vector<std::string> unknown;
    applyGraph(project.graph, project, map, "Hauptgraph", unknown);
    for (Look& look : project.looks) {
        applyGraph(look.graph, project, map, "Look " + look.name, unknown);
    }
    return unknown;
}

namespace {

void renameInGraph(graph::Graph& graph, const std::string& from, const std::string& to) {
    for (const graph::NodeInstance& node : graph.nodes()) {
        std::vector<std::string> targets = node.targets;
        bool changed = false;
        for (std::string& target : targets) {
            if (target == "fixture:" + from) {
                target = "fixture:" + to;
                changed = true;
            }
        }
        if (changed) {
            (void)graph.setTargets(node.id, std::move(targets));
        }
    }
}

}  // namespace

void renameFixtureRefs(Project& project, const std::string& from, const std::string& to) {
    for (FixtureGroup& group : project.groups) {
        std::replace(group.fixtures.begin(), group.fixtures.end(), from, to);
    }
    renameInGraph(project.graph, from, to);
    for (Look& look : project.looks) {
        renameInGraph(look.graph, from, to);
    }
}

namespace {

bool stripChains(graph::Graph& g) {
    std::vector<ConnectionId> legacy;
    for (const graph::Connection& c : g.connections()) {
        const graph::NodeInstance* from = g.findNode(c.from.node);
        const graph::NodeInstance* to = g.findNode(c.to.node);
        if (from != nullptr && to != nullptr && from->typeId == "wh.output.canvas" &&
            to->typeId == "wh.output.canvas" && c.to.portKey == "behind") {
            legacy.push_back(c.id);
        }
    }
    bool changed = false;
    for (const ConnectionId id : legacy) {
        changed = g.disconnect(id).ok() || changed;
    }
    return changed;
}

}  // namespace

bool stripOutputChains(Project& project) {
    bool changed = stripChains(project.graph);
    for (Look& look : project.looks) {
        changed = stripChains(look.graph) || changed;
    }
    return changed;
}

}  // namespace whitehole::project
