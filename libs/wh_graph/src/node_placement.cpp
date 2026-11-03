// node_placement.cpp: Umsetzung zu node_placement.hpp.
#include "whitehole/graph/node_placement.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace whitehole::graph {

namespace {

constexpr float kMargin = 30.0F;  // Abstand zwischen eingefügtem Node und seinen Nachbarn
constexpr float kClear = 20.0F;   // so nahe dürfen sich Nodes höchstens kommen

using Rects = std::unordered_map<std::uint64_t, NodeRect>;

float right(const NodeRect& r) { return r.x + r.w; }
float bottom(const NodeRect& r) { return r.y + r.h; }

bool overlaps(const NodeRect& a, const NodeRect& b) {
    return a.x < right(b) + kClear && b.x < right(a) + kClear && a.y < bottom(b) + kClear && b.y < bottom(a) + kClear;
}

/// `start` und alles, was hinter ihm hängt (über Kabel von Ausgang zu Eingang).
std::unordered_set<std::uint64_t> downstreamOf(const Graph& graph, NodeId start, bool includeStart) {
    std::unordered_set<std::uint64_t> seen{start.value()};
    std::vector<NodeId> open{start};
    while (!open.empty()) {
        const NodeId current = open.back();
        open.pop_back();
        for (const Connection& c : graph.connections()) {
            if (c.from.node == current && seen.insert(c.to.node.value()).second) {
                open.push_back(c.to.node);
            }
        }
    }
    if (!includeStart) {
        seen.erase(start.value());
    }
    return seen;
}

void shiftRight(Rects& rects, const std::unordered_set<std::uint64_t>& group, float delta) {
    if (delta <= 0.0F) {
        return;
    }
    for (const std::uint64_t id : group) {
        if (const auto it = rects.find(id); it != rects.end()) {
            it->second.x += delta;
        }
    }
}

/// Schiebt den neuen Node nach unten, bis er keinen Node mehr berührt.
void clearBelow(NodeRect& node, const Rects& rects) {
    for (int round = 0; round < 32; ++round) {
        const NodeRect* hit = nullptr;
        for (const auto& [id, other] : rects) {
            if (overlaps(node, other)) {
                hit = hit == nullptr || bottom(other) > bottom(*hit) ? &other : hit;
            }
        }
        if (hit == nullptr) {
            return;
        }
        node.y = bottom(*hit) + kClear;
    }
}

}  // namespace

Placement placeDockedNode(const Graph& graph, const std::vector<NodeRect>& rectList, const DockPlan& plan,
                          const DockTarget& target, Vec2 size, Vec2 mouse) {
    Placement result{mouse, {}};
    Rects rects;
    Rects original;
    for (const NodeRect& r : rectList) {
        rects[r.id.value()] = r;
        original[r.id.value()] = r;
    }
    NodeRect node{NodeId{}, mouse.x, mouse.y, size.x, size.y};
    std::unordered_set<std::uint64_t> pushed;  // Nodes, die Platz machen
    // Ohne berechenbare Mitte bleibt es bei der Mausposition, aber nie auf einem anderen Node
    const auto atMouse = [&]() {
        clearBelow(node, rects);
        result.position = Vec2{node.x, node.y};
        return result;
    };

    if (target.kind == DockTarget::Kind::Wire) {
        if (plan.links.size() != 2 || !rects.contains(plan.links[0].from.node.value()) ||
            !rects.contains(plan.links[1].to.node.value())) {
            return atMouse();
        }
        const NodeRect source = rects[plan.links[0].from.node.value()];
        const NodeId sinkId = plan.links[1].to.node;
        if (rects[sinkId.value()].x < right(source)) {
            return atMouse();  // Kabel läuft rückwärts: dort gibt es keine Mitte
        }
        // Platz zwischen Quelle und Ziel schaffen, dann mittig und auf halber Höhe einsetzen
        pushed = downstreamOf(graph, sinkId, true);
        const float available = rects[sinkId.value()].x - right(source);
        shiftRight(rects, pushed, std::max(0.0F, size.x + 2.0F * kMargin - available));
        const NodeRect& sink = rects[sinkId.value()];
        node.x = right(source) + (sink.x - right(source) - size.x) / 2.0F;
        node.y = (source.y + sink.y) / 2.0F;
    } else {
        if (!rects.contains(target.node.value())) {
            return atMouse();
        }
        const NodeRect host = rects[target.node.value()];
        if (!target.rightHalf) {
            // Links vor dem Ziel: Stößt der Node an etwas, rücken Ziel und alles dahinter nach rechts
            pushed = downstreamOf(graph, target.node, true);
            node.x = host.x - size.x - kMargin;
            node.y = host.y;
            float need = 0.0F;
            for (const auto& [id, other] : rects) {
                if (!pushed.contains(id) && overlaps(node, other) && other.x < host.x) {
                    need = std::max(need, right(other) + kClear - node.x);
                }
            }
            shiftRight(rects, pushed, need);
            node.x += need;
        } else {
            // Rechts hinter dem Ziel: Was hinter dem Ziel hängt und im Weg ist, rückt nach rechts
            pushed = downstreamOf(graph, target.node, false);
            node.x = right(host) + kMargin;
            node.y = host.y;
            float need = 0.0F;
            for (const std::uint64_t id : pushed) {
                const NodeRect& other = rects[id];
                if (overlaps(node, other)) {
                    need = std::max(need, right(node) + kMargin - other.x);
                }
            }
            shiftRight(rects, pushed, need);
        }
    }

    clearBelow(node, rects);
    result.position = Vec2{node.x, node.y};
    for (const std::uint64_t id : pushed) {
        const NodeRect& now = rects[id];
        if (now.x != original[id].x) {
            result.moved.emplace_back(NodeId{id}, Vec2{now.x, now.y});
        }
    }
    std::sort(result.moved.begin(), result.moved.end(),
              [](const NodeMove& a, const NodeMove& b) { return a.first.value() < b.first.value(); });
    return result;
}

}  // namespace whitehole::graph
