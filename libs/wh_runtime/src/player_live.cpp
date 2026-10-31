// player_live.cpp: Look-Liste und Live-Betrieb (Teil des Players, ausgelagert wegen der Dateigröße).
#include <cmath>

#include "whitehole/runtime/live_look.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

Error noSuchLook(int index) { return Error{ErrorCode::NotFound, "Es gibt keinen Look Nummer " + std::to_string(index + 1)}; }

bool inRange(int index, std::size_t size) { return index >= 0 && static_cast<std::size_t>(index) < size; }

}  // namespace

const std::vector<project::Look>& Player::looks() const {
    static const std::vector<project::Look> kNone;
    return project_ ? project_->looks : kNone;
}

Result<int> Player::saveLook(const std::string& name, std::optional<double> fadeSeconds) {
    if (!project_) {
        return noProject();
    }
    if (!project_->target) {
        return Error{ErrorCode::InvalidArgument, "Es gibt keinen Ausgang, aus dem ein Look entstehen könnte"};
    }
    const int existing = findLook(project_->looks, name);
    const double fade = fadeSeconds.value_or(existing >= 0 ? project_->looks[static_cast<std::size_t>(existing)].fadeSeconds : 2.0);
    auto look = captureLook(project_->graph, *project_->target, name, fade);
    if (!look.ok()) {
        return look.error();
    }
    modified_ = true;
    if (existing >= 0) {
        project_->looks[static_cast<std::size_t>(existing)] = std::move(look).value();
        return existing;
    }
    project_->looks.push_back(std::move(look).value());
    return static_cast<int>(project_->looks.size()) - 1;
}

Result<void> Player::removeLook(int index) {
    if (!project_) {
        return noProject();
    }
    if (!inRange(index, project_->looks.size())) {
        return noSuchLook(index);
    }
    if (live_) {
        return Error{ErrorCode::InvalidArgument, "Im Live-Betrieb lassen sich Looks nicht löschen"};
    }
    project_->looks.erase(project_->looks.begin() + index);
    modified_ = true;
    return {};
}

Result<void> Player::renameLook(int index, const std::string& name) {
    if (!project_) {
        return noProject();
    }
    if (!inRange(index, project_->looks.size())) {
        return noSuchLook(index);
    }
    if (!isValidLookName(name)) {
        return Error{ErrorCode::InvalidArgument, "Der Name ist leer, zu lang oder enthält Steuerzeichen"};
    }
    const int other = findLook(project_->looks, name);
    if (other >= 0 && other != index) {
        return Error{ErrorCode::AlreadyExists, "Es gibt schon einen Look mit diesem Namen"};
    }
    project_->looks[static_cast<std::size_t>(index)].name = name;
    modified_ = true;
    return {};
}

Result<void> Player::setLookFade(int index, double seconds) {
    if (!project_) {
        return noProject();
    }
    if (!inRange(index, project_->looks.size())) {
        return noSuchLook(index);
    }
    if (!std::isfinite(seconds) || seconds < 0.0 || seconds > project::kMaxFadeSeconds) {
        return Error{ErrorCode::InvalidArgument, "Die Überblendzeit muss zwischen 0 und 600 Sekunden liegen"};
    }
    project_->looks[static_cast<std::size_t>(index)].fadeSeconds = seconds;
    modified_ = true;
    return {};
}

Result<void> Player::moveLook(int from, int to) {
    if (!project_) {
        return noProject();
    }
    const std::size_t size = project_->looks.size();
    if (!inRange(from, size)) {
        return noSuchLook(from);
    }
    if (!inRange(to, size)) {
        return noSuchLook(to);
    }
    if (live_) {
        return Error{ErrorCode::InvalidArgument, "Im Live-Betrieb lässt sich die Reihenfolge nicht ändern"};
    }
    project::Look moved = std::move(project_->looks[static_cast<std::size_t>(from)]);
    project_->looks.erase(project_->looks.begin() + from);
    project_->looks.insert(project_->looks.begin() + to, std::move(moved));
    modified_ = true;
    return {};
}

Result<void> Player::loadLookIntoEditor(int index) {
    if (!project_) {
        return noProject();
    }
    if (!inRange(index, project_->looks.size())) {
        return noSuchLook(index);
    }
    if (live_) {
        return Error{ErrorCode::InvalidArgument, "Im Live-Betrieb lässt sich nichts bearbeiten; erst den Live-Betrieb beenden"};
    }
    auto before = beginEdit({});
    const project::Project backup = *project_;
    std::vector<NodeId> old;
    for (const graph::NodeInstance& node : project_->graph.nodes()) {
        old.push_back(node.id);
    }
    for (const NodeId id : old) {
        (void)project_->graph.removeNode(id);
    }
    auto placed = placeLook(project_->graph, project_->looks[static_cast<std::size_t>(index)], Vec2{});
    if (!placed.ok()) {
        *project_ = backup;
        return placed.error();
    }
    project_->target = placed.value().output;
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    rebuildDirty_ = true;  // das Ziel hat sich geändert
    return {};
}

Result<void> Player::startLiveLook(int index) {
    live_.emplace(project_->graph.registryPtr());
    liveLast_.reset();
    if (const Result<void> started = live_->start(project_->looks[static_cast<std::size_t>(index)], index); !started.ok()) {
        live_.reset();
        return started;
    }
    return {};
}

Result<void> Player::startLive(int first) {
    if (!project_) {
        return noProject();
    }
    if (project_->looks.empty()) {
        return Error{ErrorCode::InvalidArgument, "Es sind noch keine Looks gespeichert"};
    }
    if (!inRange(first, project_->looks.size())) {
        return noSuchLook(first);
    }
    if (const Result<void> started = startLiveLook(first); !started.ok()) {
        return started;
    }
    if (session_) {
        rebuildDirty_ = true;  // die Session rechnet jetzt den Graphen der Decks
    }
    return {};
}

void Player::stopLive() {
    if (!live_) {
        return;
    }
    live_.reset();
    liveLast_.reset();
    if (session_) {
        rebuildDirty_ = true;
    }
}

Result<void> Player::liveArm(int index) {
    if (!project_) {
        return noProject();
    }
    if (!live_) {
        return Error{ErrorCode::InvalidArgument, "Der Live-Betrieb ist nicht gestartet"};
    }
    if (!inRange(index, project_->looks.size())) {
        return noSuchLook(index);
    }
    return live_->arm(project_->looks[static_cast<std::size_t>(index)], index);
}

Result<void> Player::liveGo(std::optional<double> fadeSeconds) {
    if (!live_) {
        return Error{ErrorCode::InvalidArgument, "Der Live-Betrieb ist nicht gestartet"};
    }
    double seconds = 0.0;
    if (fadeSeconds) {
        seconds = *fadeSeconds;
    } else if (const int armed = live_->status().armed; inRange(armed, project_->looks.size())) {
        seconds = project_->looks[static_cast<std::size_t>(armed)].fadeSeconds;
    }
    const Result<void> go = live_->go(seconds);
    if (go.ok()) {
        liveLast_.reset();  // der Fade beginnt mit dem nächsten Frame
    }
    return go;
}

Result<void> Player::liveTrigger(int index, std::optional<double> fadeSeconds) {
    if (const Result<void> armed = liveArm(index); !armed.ok()) {
        return armed;
    }
    return liveGo(fadeSeconds);
}

LiveStatus Player::liveStatus() const noexcept { return live_ ? live_->status() : LiveStatus{}; }

}  // namespace whitehole::runtime
