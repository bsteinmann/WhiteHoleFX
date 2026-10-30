// player_edit.cpp: Graph bearbeiten (Teil des Players, ausgelagert wegen der Dateigröße).
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

constexpr std::size_t kMaxUndo = 200;

}  // namespace

std::optional<project::Project> Player::beginEdit(const std::string& key) {
    if (!key.empty() && key == lastEditKey_ && !undo_.empty()) {
        return std::nullopt;  // setzt die vorige Änderung fort: der alte Stand ist schon gemerkt
    }
    return *project_;
}

void Player::finishEdit(std::optional<project::Project> before, const std::string& key, bool ok) {
    if (!ok) {
        return;
    }
    redo_.clear();
    if (before) {
        undo_.push_back(std::move(*before));
        if (undo_.size() > kMaxUndo) {
            undo_.erase(undo_.begin());
        }
    }
    lastEditKey_ = key;
}

Result<void> Player::restored(const std::optional<graph::PortRef>& previousTarget) {
    graphDirty_ = true;
    modified_ = true;
    lastEditKey_.clear();
    if (previousTarget != project_->target) {
        targetDirty_ = true;
    }
    return {};
}

Result<void> Player::undo() {
    if (!project_ || undo_.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nichts zum Rückgängigmachen"};
    }
    const auto previousTarget = project_->target;
    redo_.push_back(std::move(*project_));
    project_.emplace(std::move(undo_.back()));
    undo_.pop_back();
    return restored(previousTarget);
}

Result<void> Player::redo() {
    if (!project_ || redo_.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nichts zum Wiederholen"};
    }
    const auto previousTarget = project_->target;
    undo_.push_back(std::move(*project_));
    project_.emplace(std::move(redo_.back()));
    redo_.pop_back();
    return restored(previousTarget);
}

Result<NodeId> Player::addNode(std::string_view typeId, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<NodeId> id = project_->graph.addNode(typeId, position);
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
    if (project_->target && project_->target->node == node) {
        return Error{ErrorCode::InvalidArgument,
                     "Dieser Node liefert den Ausgang (output); setze zuerst einen anderen Ausgang"};
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.removeNode(node);
    finishEdit(std::move(before), {}, r.ok());
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
    targetDirty_ = true;
    modified_ = true;
    return {};
}

}  // namespace whitehole::runtime
