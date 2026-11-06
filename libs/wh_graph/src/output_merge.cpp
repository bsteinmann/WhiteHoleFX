// output_merge.cpp: Umsetzung zu output_merge.hpp.
#include "whitehole/graph/output_merge.hpp"

#include <algorithm>
#include <vector>

namespace whitehole::graph {

namespace {

/// Liegt `candidate` irgendwo vor `node` (über beliebig viele Kabel)?
bool isUpstream(const Graph& graph, NodeId node, NodeId candidate) {
    std::vector<NodeId> open{node};
    std::vector<NodeId> seen;
    while (!open.empty()) {
        const NodeId current = open.back();
        open.pop_back();
        if (std::ranges::find(seen, current) != seen.end()) {
            continue;
        }
        seen.push_back(current);
        for (const Connection& c : graph.connections()) {
            if (c.to.node != current) {
                continue;
            }
            if (c.from.node == candidate) {
                return true;
            }
            open.push_back(c.from.node);
        }
    }
    return false;
}

}  // namespace

std::optional<MergedOutputs> mergeOutputs(const Graph& graph, const PortRef& target) {
    const NodeInstance* targetNode = graph.findNode(target.node);
    if (targetNode == nullptr || targetNode->typeId != kOutputNodeType) {
        return std::nullopt;
    }
    // Alte Dateien: Die frühere automatische Kette (Output.out -> Output.behind) wird auf der Kopie aufgetrennt,
    // sonst risse ein leerer Output seine Nachfolger mit.
    Graph work = graph;
    bool stripped = false;
    std::vector<ConnectionId> legacy;
    for (const Connection& c : work.connections()) {
        const NodeInstance* from = work.findNode(c.from.node);
        const NodeInstance* to = work.findNode(c.to.node);
        if (from != nullptr && to != nullptr && from->typeId == kOutputNodeType && to->typeId == kOutputNodeType &&
            c.to.portKey == "behind") {
            legacy.push_back(c.id);
        }
    }
    for (const ConnectionId id : legacy) {
        stripped = work.disconnect(id).ok() || stripped;
    }

    std::vector<NodeId> outputs;
    for (const NodeInstance& node : work.nodes()) {
        if (node.typeId == kOutputNodeType && work.findInputConnection(node.id, "source") != nullptr) {
            outputs.push_back(node.id);
        }
    }
    // Hängt ein Output über Effekte hinter einem anderen, steckt er dort bereits drin.
    std::vector<NodeId> independent;
    for (const NodeId id : outputs) {
        const bool covered = std::ranges::any_of(outputs, [&](NodeId other) { return other != id && isUpstream(work, other, id); });
        if (!covered) {
            independent.push_back(id);
        }
    }
    const bool targetHasSource = work.findInputConnection(target.node, "source") != nullptr;
    if (independent.empty() || (independent.size() < 2 && targetHasSource)) {
        if (!stripped) {
            return std::nullopt;  // nur ein Output (oder keiner mit Effekt): nichts zusammenzufassen
        }
        return MergedOutputs{std::move(work), target};
    }

    MergedOutputs merged{std::move(work), target};
    PortRef sum{independent.front(), "out"};
    for (std::size_t k = 1; k < independent.size(); ++k) {
        const Result<NodeId> add = merged.graph.addNode("wh.blend.add", Vec2{});
        if (!add.ok()) {
            return std::nullopt;
        }
        if (!merged.graph.connect(sum, PortRef{add.value(), "a"}).ok() ||
            !merged.graph.connect(PortRef{independent[k], "out"}, PortRef{add.value(), "b"}).ok()) {
            return std::nullopt;
        }
        sum = PortRef{add.value(), "out"};
    }
    merged.target = sum;
    return merged;
}

}  // namespace whitehole::graph
