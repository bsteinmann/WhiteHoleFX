// graph_layout.cpp: Spaltenanordnung für den Node-Editor.
#include "whitehole/runtime/graph_layout.hpp"

#include <algorithm>
#include <unordered_map>

namespace whitehole::runtime {

std::vector<Vec2> layerLayout(const graph::Graph& graph, const LayoutSpacing& spacing) {
    const auto& nodes = graph.nodes();
    std::unordered_map<std::uint64_t, std::size_t> indexOf;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        indexOf.emplace(nodes[i].id.value(), i);
    }
    // Spalte = Länge des längsten Wegs bis zu einer Quelle. Der Graph ist zyklenfrei
    // (Graph::connect lehnt Zyklen ab), die Rekursion endet also.
    std::vector<int> column(nodes.size(), -1);
    const auto depth = [&](auto&& self, std::size_t index) -> int {
        if (column[index] >= 0) {
            return column[index];
        }
        int best = 0;
        for (const graph::Connection& c : graph.connections()) {
            if (c.to.node == nodes[index].id) {
                best = std::max(best, self(self, indexOf.at(c.from.node.value())) + 1);
            }
        }
        column[index] = best;
        return best;
    };
    std::vector<int> rows;
    std::vector<Vec2> result(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const int col = depth(depth, i);
        if (static_cast<std::size_t>(col) >= rows.size()) {
            rows.resize(static_cast<std::size_t>(col) + 1, 0);
        }
        result[i] = Vec2{static_cast<float>(col) * spacing.column,
                         static_cast<float>(rows[static_cast<std::size_t>(col)]++) * spacing.row};
    }
    return result;
}

bool needsLayout(const graph::Graph& graph) {
    if (graph.nodes().size() < 2) {
        return false;
    }
    return std::ranges::all_of(graph.nodes(), [](const graph::NodeInstance& node) {
        return node.position.x == 0.0F && node.position.y == 0.0F;
    });
}

bool applyLayoutIfMissing(graph::Graph& graph, const LayoutSpacing& spacing) {
    if (!needsLayout(graph)) {
        return false;
    }
    const std::vector<Vec2> positions = layerLayout(graph, spacing);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        (void)graph.setPosition(graph.nodes()[i].id, positions[i]);
    }
    return true;
}

}  // namespace whitehole::runtime
