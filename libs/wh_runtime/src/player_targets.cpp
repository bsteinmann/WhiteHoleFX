// player_targets.cpp: Ziele der Output-Nodes sowie Gruppen und Regionen bearbeiten (mehrere Outputs).
//
// Warum eine eigene Datei: player_edit.cpp ist schon groß, und dies ist ein eigenes Thema. Jede Änderung ist ein
// Undo-Schritt; danach werden die Rechtecke der Output-Nodes neu berechnet (project/output_target.hpp).
#include <algorithm>
#include <unordered_set>

#include "whitehole/project/output_target.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

template <typename T>
bool uniqueNames(const std::vector<T>& items) {
    std::unordered_set<std::string> seen;
    return std::all_of(items.begin(), items.end(),
                       [&seen](const T& item) { return !item.name.empty() && seen.insert(item.name).second; });
}

}  // namespace

Result<void> Player::setOutputTargets(NodeId node, std::vector<std::string> targets) {
    if (!project_) {
        return noProject();
    }
    const graph::NodeInstance* instance = project_->graph.findNode(node);
    if (instance == nullptr) {
        return Error{ErrorCode::NotFound, "Node existiert nicht"};
    }
    if (instance->typeId != "wh.output.canvas") {
        return Error{ErrorCode::Unsupported, "Nur ein Output-Node hat ein Ziel"};
    }
    const project::Project backup = *project_;
    auto before = beginEdit({});
    if (const Result<void> r = project_->graph.setTargets(node, std::move(targets)); !r.ok()) {
        *project_ = backup;
        return r;
    }
    (void)project::applyOutputTargets(*project_, map_);
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return {};
}

Result<void> Player::setGroups(std::vector<project::FixtureGroup> groups) {
    if (!project_) {
        return noProject();
    }
    if (!uniqueNames(groups)) {
        return Error{ErrorCode::InvalidArgument, "Gruppennamen müssen eindeutig und nicht leer sein"};
    }
    for (const project::FixtureGroup& group : groups) {
        if (group.fixtures.empty()) {
            return Error{ErrorCode::InvalidArgument, "Eine Gruppe braucht mindestens ein Fixture"};
        }
    }
    auto before = beginEdit({});
    project_->groups = std::move(groups);
    (void)project::applyOutputTargets(*project_, map_);
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return {};
}

Result<void> Player::setRegions(std::vector<project::StageRegion> regions) {
    if (!project_) {
        return noProject();
    }
    if (!uniqueNames(regions)) {
        return Error{ErrorCode::InvalidArgument, "Regionsnamen müssen eindeutig und nicht leer sein"};
    }
    for (const project::StageRegion& r : regions) {
        const bool inside = r.left >= 0.0F && r.top >= 0.0F && r.width > 0.0F && r.height > 0.0F &&
                            r.left + r.width <= 1.0001F && r.top + r.height <= 1.0001F;
        if (!inside) {
            return Error{ErrorCode::InvalidArgument, "Eine Region muss in der Leinwand (0 bis 1) liegen"};
        }
    }
    auto before = beginEdit({});
    project_->regions = std::move(regions);
    (void)project::applyOutputTargets(*project_, map_);
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return {};
}

}  // namespace whitehole::runtime
