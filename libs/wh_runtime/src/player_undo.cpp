// player_undo.cpp: Rückgängig/Wiederholen (Teil des Players, ausgelagert wegen der Dateigröße).
//
// Verfahren: Vor einer Änderung wird eine Kopie des ganzen Projekts auf den Undo-Stapel gelegt.
// Das ist einfach und fehlerarm; ein Projekt ist klein (Fixtures sind Beschreibungen, keine LED-
// Listen). Gleichartige Änderungen in Folge (Regler ziehen) teilen sich eine Kopie.
#include "whitehole/runtime/player.hpp"

#include "whitehole/output/packet_sink.hpp"

namespace whitehole::runtime {

namespace {

constexpr std::size_t kMaxUndo = 200;

class DiscardSink final : public output::IPacketSink {
public:
    Result<void> send(const output::Endpoint&, std::span<const std::uint8_t>) override { return {}; }
};

}  // namespace

std::optional<project::Project> Player::beginEdit(const std::string& key) {
    if (!key.empty() && key == lastEditKey_ && !undo_.empty()) {
        return std::nullopt;  // setzt die vorige Änderung fort: der alte Stand ist schon gemerkt
    }
    return *project_;
}

void Player::finishEdit(std::optional<project::Project> before, const std::string& key, bool ok, bool rebuild) {
    if (!ok) {
        return;
    }
    redo_.clear();
    if (before) {
        undo_.push_back(Snapshot{std::move(*before), rebuild});
        if (undo_.size() > kMaxUndo) {
            undo_.erase(undo_.begin());
        }
    } else if (rebuild && !undo_.empty()) {
        undo_.back().rebuild = true;
    }
    lastEditKey_ = key;
}

Result<void> Player::restored(const std::optional<graph::PortRef>& previousTarget, bool rebuild) {
    graphDirty_ = true;
    modified_ = true;
    lastEditKey_.clear();
    if (previousTarget != project_->target) {
        rebuildDirty_ = true;
    }
    if (rebuild) {
        // Layout oder Ausgabe haben sich geändert: Vorschau-Map neu berechnen, Session neu aufbauen.
        DiscardSink discard;
        auto parts = buildOutput(*project_, discard);
        if (!parts.ok()) {
            return parts.error();
        }
        map_ = std::move(parts.value().map);
        rebuildDirty_ = true;
    }
    return {};
}

Result<void> Player::undo() {
    if (!project_ || undo_.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nichts zum Rückgängigmachen"};
    }
    const auto previousTarget = project_->target;
    Snapshot top = std::move(undo_.back());
    undo_.pop_back();
    auto looks = std::move(project_->looks);  // die Look-Liste gehört nicht zum Rückgängig-Verlauf
    redo_.push_back(Snapshot{std::move(*project_), top.rebuild});
    project_.emplace(std::move(top.project));
    project_->looks = std::move(looks);
    return restored(previousTarget, top.rebuild);
}

Result<void> Player::redo() {
    if (!project_ || redo_.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nichts zum Wiederholen"};
    }
    const auto previousTarget = project_->target;
    Snapshot top = std::move(redo_.back());
    redo_.pop_back();
    auto looks = std::move(project_->looks);
    undo_.push_back(Snapshot{std::move(*project_), top.rebuild});
    project_.emplace(std::move(top.project));
    project_->looks = std::move(looks);
    return restored(previousTarget, top.rebuild);
}

Result<void> Player::editProject(const std::string& key,
                                 const std::function<Result<void>(project::Project&)>& edit) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    project::Project backup = *project_;
    auto before = beginEdit(key);
    Result<void> r = edit(*project_);
    DiscardSink discard;
    std::optional<mapping::LedMap> map;
    if (r.ok()) {
        auto parts = buildOutput(*project_, discard);
        if (parts.ok()) {
            map = std::move(parts.value().map);
        } else {
            r = parts.error();
        }
    }
    if (!r.ok()) {
        *project_ = std::move(backup);
        return r;
    }
    map_ = std::move(*map);
    finishEdit(std::move(before), key, true, true);
    modified_ = true;
    rebuildDirty_ = true;
    return {};
}

}  // namespace whitehole::runtime
