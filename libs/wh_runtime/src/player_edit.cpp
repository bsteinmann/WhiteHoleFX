// player_edit.cpp: Graph bearbeiten (Teil des Players, ausgelagert wegen der Dateigröße).
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

}  // namespace

Result<NodeId> Player::addNode(std::string_view typeId, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    const Result<NodeId> id = project_->graph.addNode(typeId, position);
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
    if (project_->target && project_->target->node == node) {
        return Error{ErrorCode::InvalidArgument,
                     "Dieser Node liefert den Ausgang (output); setze zuerst einen anderen Ausgang"};
    }
    const Result<void> r = project_->graph.removeNode(node);
    if (r.ok()) {
        modified_ = true;
        graphDirty_ = true;
    }
    return r;
}

Result<ConnectionId> Player::connect(const graph::PortRef& from, const graph::PortRef& to) {
    if (!project_) {
        return noProject();
    }
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
    modified_ = true;
    graphDirty_ = true;
    return id;
}

Result<void> Player::disconnect(ConnectionId connection) {
    if (!project_) {
        return noProject();
    }
    const Result<void> r = project_->graph.disconnect(connection);
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
    const Result<void> r = project_->graph.setPosition(node, position);
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
    project_->target = port;
    targetDirty_ = true;
    modified_ = true;
    return {};
}

}  // namespace whitehole::runtime
