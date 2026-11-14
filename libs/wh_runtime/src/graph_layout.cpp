// graph_layout.cpp: Spaltenanordnung für den Node-Editor.
#include "whitehole/runtime/graph_layout.hpp"

#include <algorithm>
#include <unordered_map>

namespace whitehole::runtime {

namespace {
// Spiegelt NodeItem::sizeFor() aus apps/whitehole_studio/node_items.cpp (kNodeTitleHeight, kNodeRowHeight) rein
// zahlenmäßig, ohne Qt-Abhängigkeit (dieses Modul bleibt bewusst Qt-frei, siehe Kopfkommentar der .hpp). Ändert
// sich dort Titelhöhe oder Zeilenhöhe der Node-Karte, hier nachziehen - sonst überlappen hochgewachsene Nodes
// (mehr Parameter = mehr automatische Eingangsports = höhere Karte) wieder ihren Nachbarn in derselben Spalte.
constexpr float kNodeTitleHeight = 26.0F;
constexpr float kNodeRowHeight = 22.0F;

/// Geschätzte Kartenhöhe eines Node-Typs: Titel plus eine Zeile je Ein-/Ausgang (das Maximum beider Seiten),
/// mindestens eine Zeile. Unbekannter Typ (sollte nicht vorkommen) liefert die Mindesthöhe.
float estimatedHeight(const graph::NodeDescriptor* descriptor) {
    int inputs = 0;
    int outputs = 0;
    if (descriptor != nullptr) {
        for (const graph::PortDescriptor& port : descriptor->ports) {
            (port.direction == graph::PortDirection::Output ? outputs : inputs) += 1;
        }
    }
    return kNodeTitleHeight + 12.0F + static_cast<float>(std::max(1, std::max(inputs, outputs))) * kNodeRowHeight;
}
}  // namespace

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
    // Nächste freie Y-Position je Spalte: wächst um die tatsächlich geschätzte Höhe des zuletzt platzierten Nodes
    // plus `spacing.row` als Abstand - so bleibt genug Platz, auch wenn ein Node (viele Parameter) hoch ist.
    std::vector<float> columnY;
    std::vector<Vec2> result(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const int col = depth(depth, i);
        if (static_cast<std::size_t>(col) >= columnY.size()) {
            columnY.resize(static_cast<std::size_t>(col) + 1, 0.0F);
        }
        const float y = columnY[static_cast<std::size_t>(col)];
        result[i] = Vec2{static_cast<float>(col) * spacing.column, y};
        columnY[static_cast<std::size_t>(col)] =
            y + estimatedHeight(graph.registry().find(nodes[i].typeId)) + spacing.row;
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
