// player_groups.cpp: Gruppen im Player bearbeiten (Teil des Players, ausgelagert wegen der Dateigröße).
//
// Warum es das gibt: Der Node-Editor ändert den Graphen nie selbst, jede Änderung läuft über den Player, damit
// Rückgängig und "geändert"-Markierung an einer Stelle stimmen. Gruppen ändern nur die Oberfläche, darum bleibt der
// Rechenkern unberührt (kein graphDirty_, kein Neubau des Shaders).
#include "whitehole/runtime/player.hpp"

#include <algorithm>

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

}  // namespace

Result<void> Player::addNodeGroup(graph::NodeGroup group) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.addGroup(std::move(group));
    finishEdit(std::move(before), {}, r.ok());
    modified_ = modified_ || r.ok();
    return r;
}

Result<void> Player::replaceNodeGroup(const std::string& name, graph::NodeGroup group) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.replaceGroup(name, std::move(group));
    finishEdit(std::move(before), {}, r.ok());
    modified_ = modified_ || r.ok();
    return r;
}

Result<void> Player::removeNodeGroup(const std::string& name) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.removeGroup(name);
    finishEdit(std::move(before), {}, r.ok());
    modified_ = modified_ || r.ok();
    return r;
}

Result<void> Player::setNodeGroupCollapsed(const std::string& name, bool collapsed) {
    if (!project_) {
        return noProject();
    }
    const graph::NodeGroup* group = project_->graph.findGroup(name);
    if (group == nullptr) {
        return Error{ErrorCode::NotFound, "Gruppe existiert nicht"};
    }
    // Die Kachel sitzt an der linken oberen Ecke der Nodes. Beim Zuklappen merkt sie sich diese Ecke; beim Aufklappen
    // wandern die Nodes dorthin, wo die Kachel inzwischen steht. Alles ein Schritt Rückgängig.
    float minX = 0.0F;
    float minY = 0.0F;
    bool first = true;
    for (const NodeId id : group->members) {
        if (const graph::NodeInstance* node = project_->graph.findNode(id)) {
            minX = first ? node->position.x : std::min(minX, node->position.x);
            minY = first ? node->position.y : std::min(minY, node->position.y);
            first = false;
        }
    }
    const std::vector<NodeId> members = group->members;
    const Vec2 tile = group->position;
    auto before = beginEdit({});
    Result<void> r = project_->graph.setGroupCollapsed(name, collapsed);
    if (r.ok() && collapsed) {
        r = project_->graph.setGroupPosition(name, Vec2{minX, minY});
    } else if (r.ok() && !first) {
        for (const NodeId id : members) {
            if (const graph::NodeInstance* node = project_->graph.findNode(id)) {
                const Vec2 moved{node->position.x - minX + tile.x, node->position.y - minY + tile.y};
                (void)project_->graph.setPosition(id, moved);
            }
        }
    }
    finishEdit(std::move(before), {}, r.ok());
    modified_ = modified_ || r.ok();
    return r;
}

Result<void> Player::moveNodeGroup(const std::string& name, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    const std::string editKey = "movegroup:" + name;
    auto before = beginEdit(editKey);
    const Result<void> r = project_->graph.setGroupPosition(name, position);
    finishEdit(std::move(before), editKey, r.ok());
    modified_ = modified_ || r.ok();
    return r;
}

}  // namespace whitehole::runtime
