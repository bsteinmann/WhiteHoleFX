// player_nodes.cpp: die Node-Liste des Editor-Graphen für das Parameter-Dock (Teil des Players).
//
// Warum es diese Datei gibt: ausgelagert aus player.cpp wegen der Dateigröße (dort kam das Vorab-Übersetzen im
// Live-Betrieb hinzu). Inhaltlich unverändert.
#include "whitehole/runtime/node_display_name.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

std::vector<NodeInfo> Player::nodes() const {
    std::vector<NodeInfo> result;
    if (!project_) {
        return result;
    }
    const graph::Graph& g = project_->graph;
    const auto names = nodeDisplayNames(g);  // Typ+Nummer, plus eigener Name in Klammern, siehe node_display_name.hpp
    for (const graph::NodeInstance& node : g.nodes()) {
        const graph::NodeDescriptor* descriptor = g.registry().find(node.typeId);
        // Auch Nodes ohne Parameter (z. B. Add) erscheinen: Das Dock zeigt dann einen Hinweis, statt sie zu verschweigen.
        if (descriptor == nullptr) {
            continue;
        }
        NodeInfo info;
        info.id = node.id;
        info.typeId = node.typeId;
        const auto nameIt = names.find(node.id.value());
        info.title = nameIt != names.end() ? nameIt->second : descriptor->displayName;
        for (const graph::ParameterDescriptor& p : descriptor->parameters) {
            ParameterInfo parameter;
            parameter.key = p.key;
            parameter.displayName = p.displayName;
            parameter.defaultValue = p.defaultValue;
            const auto it = node.parameters.find(p.key);
            parameter.value = it != node.parameters.end() ? it->second : p.defaultValue;
            parameter.minValue = p.minValue;
            parameter.maxValue = p.maxValue;
            parameter.choices = p.choices;
            parameter.visibleWhenAtLeastKey = p.visibleWhenAtLeastKey;
            parameter.visibleWhenAtLeast = p.visibleWhenAtLeast;
            if (const graph::Connection* wire = g.findInputConnection(node.id, p.key)) {
                parameter.driven = true;
                parameter.connection = wire->id;
                parameter.scale = wire->scale;
                parameter.offset = wire->offset;
            }
            info.parameters.push_back(std::move(parameter));
        }
        result.push_back(std::move(info));
    }
    return result;
}

}  // namespace whitehole::runtime
