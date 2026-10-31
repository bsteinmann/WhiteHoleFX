// live_look.cpp: Looks aus einem Graphen lösen und einsetzen.
#include "whitehole/runtime/live_look.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "whitehole/runtime/node_clipboard.hpp"

namespace whitehole::runtime {

bool isValidLookName(std::string_view name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    return std::ranges::none_of(name, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; });
}

int findLook(const std::vector<project::Look>& looks, std::string_view name) noexcept {
    for (std::size_t i = 0; i < looks.size(); ++i) {
        if (looks[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Result<project::Look> captureLook(const graph::Graph& graph, const graph::PortRef& target, std::string name,
                                  double fadeSeconds) {
    if (!isValidLookName(name)) {
        return Error{ErrorCode::InvalidArgument, "Der Name des Looks ist leer, zu lang oder enthält Steuerzeichen"};
    }
    if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0 || fadeSeconds > project::kMaxFadeSeconds) {
        return Error{ErrorCode::InvalidArgument, "Die Überblendzeit muss zwischen 0 und 600 Sekunden liegen"};
    }
    if (graph.findNode(target.node) == nullptr) {
        return Error{ErrorCode::NotFound, "Der Ausgang liegt nicht im Graphen"};
    }

    // Rückwärts über die Kabel: alles, was den Ausgang speist
    std::unordered_set<std::uint64_t> keep{target.node.value()};
    std::vector<NodeId> open{target.node};
    while (!open.empty()) {
        const NodeId current = open.back();
        open.pop_back();
        for (const graph::Connection& c : graph.connections()) {
            if (c.to.node == current && keep.insert(c.from.node.value()).second) {
                open.push_back(c.from.node);
            }
        }
    }
    std::vector<NodeId> selection;
    std::size_t targetIndex = 0;
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (keep.contains(node.id.value())) {
            if (node.id == target.node) {
                targetIndex = selection.size();
            }
            selection.push_back(node.id);
        }
    }

    project::Look look{graph.registryPtr()};
    look.name = std::move(name);
    look.fadeSeconds = fadeSeconds;
    const NodeClipboard clipboard = copyNodes(graph, selection);
    auto ids = applyClipboard(look.graph, clipboard, Vec2{});
    if (!ids.ok()) {
        return ids.error();
    }
    look.target = graph::PortRef{ids.value()[targetIndex], target.portKey};
    return look;
}

Result<PlacedLook> placeLook(graph::Graph& graph, const project::Look& look, Vec2 offset) {
    std::vector<NodeId> selection;
    std::size_t targetIndex = 0;
    for (const graph::NodeInstance& node : look.graph.nodes()) {
        if (node.id == look.target.node) {
            targetIndex = selection.size();
        }
        selection.push_back(node.id);
    }
    if (selection.empty()) {
        return Error{ErrorCode::InvalidArgument, "Der Look \"" + look.name + "\" ist leer"};
    }
    auto ids = applyClipboard(graph, copyNodes(look.graph, selection), offset);
    if (!ids.ok()) {
        return ids.error();
    }
    PlacedLook placed;
    placed.output = graph::PortRef{ids.value()[targetIndex], look.target.portKey};
    placed.nodes = std::move(ids).value();
    return placed;
}

}  // namespace whitehole::runtime
