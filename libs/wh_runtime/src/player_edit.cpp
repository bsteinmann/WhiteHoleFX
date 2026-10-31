// player_edit.cpp: Graph bearbeiten (Teil des Players, ausgelagert wegen der Dateigröße).
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

/// Der Field-Ausgang, der in `node` hineinläuft (erste Verbindung in Reihenfolge des Graphen).
std::optional<graph::PortRef> upstreamFieldOutput(const graph::Graph& graph, NodeId node) {
    for (const graph::Connection& c : graph.connections()) {
        if (c.to.node != node || c.from.node == node) {
            continue;
        }
        const graph::NodeInstance* source = graph.findNode(c.from.node);
        const graph::NodeDescriptor* descriptor = source ? graph.registry().find(source->typeId) : nullptr;
        const graph::PortDescriptor* port = descriptor ? descriptor->findPort(c.from.portKey) : nullptr;
        if (port != nullptr && port->type == graph::PortType::Field) {
            return c.from;
        }
    }
    return std::nullopt;
}

}  // namespace

Result<NodeId> Player::addNode(std::string_view typeId, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<NodeId> id = project_->graph.addNode(typeId, position);
    if (id.ok() && typeId == "wh.output.canvas") {
        // Ein neuer Ausgangs-Node wird zum Ausgang des Projekts und hängt sich hinter den bisherigen
        // (wenn der Eingang frei ist, was bei einem neuen Node immer gilt).
        if (project_->target) {
            (void)project_->graph.connect(*project_->target, graph::PortRef{id.value(), "source"});
        }
        project_->target = graph::PortRef{id.value(), "out"};
        rebuildDirty_ = true;
    }
    finishEdit(std::move(before), {}, id.ok());
    if (id.ok()) {
        modified_ = true;
        graphDirty_ = true;
    }
    return id;
}

Result<void> Player::removeNode(NodeId node) {
    if (!project_) {
        return noProject();
    }
    // Liefert der Node den Ausgang, rückt der Ausgang auf den Node davor (den ersten Field-Eingang,
    // der belegt ist). Gibt es keinen, bleibt das Projekt ohne Ausgang; das Starten meldet es dann.
    const bool wasTarget = project_->target && project_->target->node == node;
    std::optional<graph::PortRef> upstream;
    if (wasTarget) {
        upstream = upstreamFieldOutput(project_->graph, node);
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.removeNode(node);
    if (r.ok() && wasTarget) {
        project_->target = upstream;
    }
    finishEdit(std::move(before), {}, r.ok());
    if (r.ok()) {
        modified_ = true;
        graphDirty_ = true;
        if (wasTarget) {
            rebuildDirty_ = true;  // das Ziel steckt in der Stage
        }
    }
    return r;
}

Result<ConnectionId> Player::connect(const graph::PortRef& from, const graph::PortRef& to) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    graph::Graph& g = project_->graph;
    // Eine belegte Eingabe wird ersetzt. Die alte Verbindung merken wir uns, damit ein
    // Fehlschlag (Typ, Zyklus) nichts zerstört.
    std::optional<graph::Connection> previous;
    if (const graph::Connection* existing = g.findInputConnection(to.node, to.portKey)) {
        previous = *existing;
        (void)g.disconnect(existing->id);
    }
    Result<ConnectionId> id = g.connect(from, to);
    if (!id.ok()) {
        if (previous) {
            (void)g.connect(previous->from, previous->to);
        }
        return id;
    }
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return id;
}

Result<void> Player::disconnect(ConnectionId connection) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.disconnect(connection);
    finishEdit(std::move(before), {}, r.ok());
    if (r.ok()) {
        modified_ = true;
        graphDirty_ = true;
    }
    return r;
}

Result<void> Player::moveNode(NodeId node, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    const std::string editKey = "move:" + std::to_string(node.value());
    auto before = beginEdit(editKey);
    const Result<void> r = project_->graph.setPosition(node, position);
    finishEdit(std::move(before), editKey, r.ok());
    if (r.ok()) {
        modified_ = true;
    }
    return r;
}

Result<void> Player::setTarget(const graph::PortRef& port) {
    if (!project_) {
        return noProject();
    }
    const graph::NodeInstance* instance = project_->graph.findNode(port.node);
    if (instance == nullptr) {
        return Error{ErrorCode::NotFound, "Node existiert nicht"};
    }
    const graph::NodeDescriptor* descriptor = project_->graph.registry().find(instance->typeId);
    const graph::PortDescriptor* found = descriptor ? descriptor->findPort(port.portKey) : nullptr;
    if (found == nullptr) {
        return Error{ErrorCode::NotFound, "Port existiert nicht: " + port.portKey};
    }
    if (found->direction != graph::PortDirection::Output || found->type != graph::PortType::Field) {
        return Error{ErrorCode::IncompatiblePorts, "Als Ausgang taugt nur ein Field-Ausgang"};
    }
    if (project_->target && *project_->target == port) {
        return {};
    }
    auto before = beginEdit({});
    project_->target = port;
    finishEdit(std::move(before), {}, true);
    rebuildDirty_ = true;
    modified_ = true;
    return {};
}

}  // namespace whitehole::runtime
