#include "whitehole/graph/graph_model.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "whitehole/graph/graph_topology.hpp"
#include "whitehole/graph/port_type.hpp"

namespace whitehole::graph {

namespace {

/// Begrenzt Zahlenwerte auf den Bereich des Parameters; andere Typen bleiben unverändert.
ParameterValue clampToRange(ParameterValue value, const ParameterDescriptor& descriptor) {
    if (auto* number = std::get_if<double>(&value)) {
        if (descriptor.minValue.has_value()) {
            *number = std::max(*number, *descriptor.minValue);
        }
        if (descriptor.maxValue.has_value()) {
            *number = std::min(*number, *descriptor.maxValue);
        }
    } else if (auto* integer = std::get_if<std::int64_t>(&value)) {
        if (descriptor.minValue.has_value()) {
            *integer = std::max(*integer, static_cast<std::int64_t>(std::ceil(*descriptor.minValue)));
        }
        if (descriptor.maxValue.has_value()) {
            *integer = std::min(*integer, static_cast<std::int64_t>(std::floor(*descriptor.maxValue)));
        }
    }
    return value;
}

bool isFinite(const ParameterValue& value) {
    const auto finite = [](float number) { return std::isfinite(number); };
    if (const auto* number = std::get_if<double>(&value)) {
        return std::isfinite(*number);
    }
    if (const auto* v2 = std::get_if<Vec2>(&value)) {
        return finite(v2->x) && finite(v2->y);
    }
    if (const auto* v3 = std::get_if<Vec3>(&value)) {
        return finite(v3->x) && finite(v3->y) && finite(v3->z);
    }
    if (const auto* color = std::get_if<ColorRgba>(&value)) {
        return finite(color->r) && finite(color->g) && finite(color->b) && finite(color->a);
    }
    return true;
}

Error notFound(const std::string& what) {
    return Error{ErrorCode::NotFound, what};
}

}  // namespace

Graph::Graph(std::shared_ptr<const INodeRegistry> registry) : registry_{std::move(registry)} {
    assert(registry_ != nullptr && "Graph braucht ein Node-Verzeichnis");
}

Result<NodeId> Graph::addNode(std::string_view typeId, Vec2 position) {
    const NodeDescriptor* descriptor = registry_->find(typeId);
    if (descriptor == nullptr) {
        return notFound("Unbekannter Node-Typ: " + std::string{typeId});
    }

    NodeInstance instance;
    instance.id = nodeIds_.next();
    instance.typeId = descriptor->typeId;
    instance.label = descriptor->displayName;
    instance.position = position;
    for (const ParameterDescriptor& parameter : descriptor->parameters) {
        instance.parameters.emplace(parameter.key, parameter.defaultValue);
    }

    const NodeId id = instance.id;
    nodes_.push_back(std::move(instance));
    return id;
}

Result<void> Graph::removeNode(NodeId id) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const NodeInstance& node) { return node.id == id; });
    if (it == nodes_.end()) {
        return notFound("Node existiert nicht");
    }

    std::erase_if(connections_, [id](const Connection& connection) {
        return connection.from.node == id || connection.to.node == id;
    });
    nodes_.erase(it);
    return {};
}

Result<ConnectionId> Graph::connect(const PortRef& from, const PortRef& to) {
    const NodeInstance* fromNode = findNode(from.node);
    const NodeInstance* toNode = findNode(to.node);
    if (fromNode == nullptr || toNode == nullptr) {
        return notFound("Quell- oder Ziel-Node existiert nicht");
    }

    const NodeDescriptor* fromDescriptor = registry_->find(fromNode->typeId);
    const NodeDescriptor* toDescriptor = registry_->find(toNode->typeId);
    if (fromDescriptor == nullptr || toDescriptor == nullptr) {
        return notFound("Node-Typ ist nicht mehr registriert");
    }

    const PortDescriptor* fromPort = fromDescriptor->findPort(from.portKey);
    const PortDescriptor* toPort = toDescriptor->findPort(to.portKey);
    if (fromPort == nullptr || toPort == nullptr) {
        return notFound("Port existiert nicht: " + (fromPort == nullptr ? from.portKey : to.portKey));
    }

    if (fromPort->direction != PortDirection::Output || toPort->direction != PortDirection::Input) {
        return Error{ErrorCode::IncompatiblePorts, "Verbindungen führen von einem Ausgang zu einem Eingang"};
    }
    if (!isConnectable(fromPort->type, toPort->type)) {
        return Error{ErrorCode::IncompatiblePorts, "Die Porttypen sind nicht verbindbar"};
    }
    if (isReachable(connections_, to.node, from.node)) {
        return Error{ErrorCode::CycleDetected, "Die Verbindung würde einen Zyklus erzeugen"};
    }
    if (findInputConnection(to.node, to.portKey) != nullptr) {
        return Error{ErrorCode::AlreadyExists, "Der Eingang ist bereits verbunden"};
    }

    const ConnectionId id = connectionIds_.next();  // erst nach erfolgreicher Prüfung vergeben
    connections_.push_back(Connection{id, from, to});
    return id;
}

Result<void> Graph::disconnect(ConnectionId id) {
    const auto removed = std::erase_if(
        connections_, [id](const Connection& connection) { return connection.id == id; });
    if (removed == 0) {
        return notFound("Verbindung existiert nicht");
    }
    return {};
}

Result<void> Graph::setParameter(NodeId id, std::string_view key, ParameterValue value) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const NodeInstance& node) { return node.id == id; });
    if (it == nodes_.end()) {
        return notFound("Node existiert nicht");
    }

    const NodeDescriptor* descriptor = registry_->find(it->typeId);
    const ParameterDescriptor* parameter =
        descriptor == nullptr ? nullptr : descriptor->findParameter(key);
    if (parameter == nullptr) {
        return notFound("Parameter existiert nicht: " + std::string{key});
    }

    if (value.index() != parameter->defaultValue.index()) {
        return Error{ErrorCode::InvalidArgument,
                     "Falscher Wertetyp für Parameter '" + std::string{key} + "'"};
    }
    if (!isFinite(value)) {
        return Error{ErrorCode::InvalidArgument,
                     "Parameter '" + std::string{key} + "' braucht endliche Zahlen"};
    }

    it->parameters.insert_or_assign(std::string{key}, clampToRange(std::move(value), *parameter));
    return {};
}

Result<void> Graph::setPosition(NodeId id, Vec2 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        return Error{ErrorCode::InvalidArgument, "Die Position muss endlich sein"};
    }
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const NodeInstance& node) { return node.id == id; });
    if (it == nodes_.end()) {
        return notFound("Node existiert nicht");
    }
    it->position = position;
    return {};
}

const NodeInstance* Graph::findNode(NodeId id) const noexcept {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [id](const NodeInstance& node) { return node.id == id; });
    return it == nodes_.end() ? nullptr : &*it;
}

const Connection* Graph::findConnection(ConnectionId id) const noexcept {
    const auto it = std::find_if(connections_.begin(), connections_.end(),
                                 [id](const Connection& connection) { return connection.id == id; });
    return it == connections_.end() ? nullptr : &*it;
}

const Connection* Graph::findInputConnection(NodeId node, std::string_view portKey) const noexcept {
    const auto it = std::find_if(connections_.begin(), connections_.end(),
                                 [&](const Connection& connection) {
                                     return connection.to.node == node && connection.to.portKey == portKey;
                                 });
    return it == connections_.end() ? nullptr : &*it;
}

}  // namespace whitehole::graph
