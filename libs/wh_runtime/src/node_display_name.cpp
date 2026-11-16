// node_display_name.cpp: Umsetzung zu node_display_name.hpp.
#include "whitehole/runtime/node_display_name.hpp"

namespace whitehole::runtime {

std::unordered_map<std::uint64_t, std::string> nodeDisplayNames(const graph::Graph& graph) {
    std::unordered_map<std::uint64_t, std::string> result;
    std::unordered_map<std::string, std::size_t> total;
    for (const graph::NodeInstance& node : graph.nodes()) {
        ++total[node.typeId];
    }
    std::unordered_map<std::string, std::size_t> seen;
    for (const graph::NodeInstance& node : graph.nodes()) {
        const std::size_t index = ++seen[node.typeId];
        const graph::NodeDescriptor* descriptor = graph.registry().find(node.typeId);
        if (descriptor == nullptr) {
            // Unbekannter Typ: kein Automatikname zum Anhängen - dann wenigstens der eigene Name, falls
            // gesetzt, genau wie zuvor.
            if (!node.label.empty()) {
                result.emplace(node.id.value(), node.label);
            }
            continue;
        }
        std::string name = descriptor->displayName;
        if (total[node.typeId] > 1) {
            name += " " + std::to_string(index);
        }
        // Eigener Name steht zusätzlich zum Automatiknamen, nicht statt ihm (vom Nutzer gewünscht): "DMX
        // Channel 2 (Eingangstür)" statt nur "Eingangstür" - man sieht weiterhin auf einen Blick, was für
        // ein Node das ist, auch mit eigenem Namen.
        if (!node.label.empty()) {
            name += " (" + node.label + ")";
        }
        result.emplace(node.id.value(), std::move(name));
    }
    return result;
}

std::string nodeDisplayName(const graph::Graph& graph, NodeId id) {
    const auto names = nodeDisplayNames(graph);
    const auto it = names.find(id.value());
    return it != names.end() ? it->second : std::string{};
}

}  // namespace whitehole::runtime
