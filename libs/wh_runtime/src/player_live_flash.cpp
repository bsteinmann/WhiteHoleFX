// player_live_flash.cpp: Looks im Live-Betrieb drücken und loslassen - umschalten oder flashen (Teil des Players).
//
// Warum es diese Datei gibt: Ob ein Look beim Loslassen zurückkehrt, ist eine Eigenschaft des Looks (project::Look::trigger),
// nicht des Eingangs. Darum steckt die Logik hier, einmal für alle Quellen: Pads am MIDI-Controller, Knöpfe im
// Live-Fenster, später DMX-Schwellen. Sie rufen nur liveDown/liveUp. Entwurf: docs/entwurf-midi.md, Abschnitt 5.
//
// Regeln: Flash hat Vorrang vor "Mit GO". Beim ersten gedrückten Flash-Look wird der sichtbare Look gemerkt; mehrere
// gehaltene Flash-Looks stapeln sich (der zuletzt gedrückte ist zu sehen), erst das Loslassen des letzten kehrt zurück.
// Schaltet inzwischen jemand um (liveTrigger/liveArm/liveGo), endet das Flash und der neue Look bleibt stehen.
// Zeiten: Einblenden beim Drücken = `in` des Looks, sonst die eigene Zeit des Live-Fensters, sonst die Überblendzeit des
// Looks; Zurückblenden beim Loslassen = `return` des Looks, sonst dessen Überblendzeit.
#include <algorithm>
#include <cmath>

#include "whitehole/runtime/live_look.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

bool inRange(int index, std::size_t size) { return index >= 0 && static_cast<std::size_t>(index) < size; }

}  // namespace

Result<void> Player::setLookTrigger(int index, project::LookTrigger trigger, std::optional<double> returnSeconds) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (!inRange(index, project_->looks.size())) {
        return Error{ErrorCode::NotFound, "Es gibt keinen Look Nummer " + std::to_string(index + 1)};
    }
    if (returnSeconds && !(std::isfinite(*returnSeconds) && *returnSeconds >= 0.0 && *returnSeconds <= project::kMaxFadeSeconds)) {
        return Error{ErrorCode::InvalidArgument, "Die Zeit zum Zurückblenden muss zwischen 0 und 600 Sekunden liegen"};
    }
    project::Look& look = project_->looks[static_cast<std::size_t>(index)];
    const bool dropsIn = trigger == project::LookTrigger::Toggle && look.flashInSeconds.has_value();
    if (look.trigger == trigger && look.returnSeconds == returnSeconds && !dropsIn) {
        return {};
    }
    if (trigger != project::LookTrigger::Flash) {
        std::erase_if(flashHeld_, [index](const HeldFlash& held) { return held.key == index; });  // ein gehaltener Look hört auf zu flashen; das Bild bleibt, wie es ist
    }
    look.trigger = trigger;
    look.returnSeconds = returnSeconds;
    if (trigger == project::LookTrigger::Toggle) {
        look.flashInSeconds.reset();  // ohne Flash gibt es kein Einblenden beim Drücken
    }
    modified_ = true;
    return {};
}

Result<void> Player::setLookFlashIn(int index, std::optional<double> seconds) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (!inRange(index, project_->looks.size())) {
        return Error{ErrorCode::NotFound, "Es gibt keinen Look Nummer " + std::to_string(index + 1)};
    }
    if (seconds && !(std::isfinite(*seconds) && *seconds >= 0.0 && *seconds <= project::kMaxFadeSeconds)) {
        return Error{ErrorCode::InvalidArgument, "Die Zeit zum Einblenden muss zwischen 0 und 600 Sekunden liegen"};
    }
    project::Look& look = project_->looks[static_cast<std::size_t>(index)];
    if (look.flashInSeconds != seconds) {
        look.flashInSeconds = seconds;
        modified_ = true;
    }
    return {};
}

Result<void> Player::flashDown(int key, int look, std::optional<double> flashIn, double back,
                               std::optional<double> fadeSeconds) {
    if (flashHeld_.empty()) {
        const LiveStatus status = live_->status();
        flashReturn_ = status.fading ? status.incoming : status.current;  // was gerade zu sehen ist bzw. kommt
    }
    std::erase_if(flashHeld_, [key](const HeldFlash& held) { return held.key == key; });
    flashHeld_.push_back({key, look, back});
    if (const Result<void> armed = armLook(look); !armed.ok()) {
        return armed;
    }
    // Die eigene Einblendzeit gilt vor der Zeit des Live-Fensters und der des Looks
    return goLook(flashIn ? std::optional<double>{*flashIn} : fadeSeconds);
}

Result<void> Player::flashUp(int key) {
    if (!project_ || !live_) {
        return {};
    }
    const auto found = std::find_if(flashHeld_.begin(), flashHeld_.end(), [key](const HeldFlash& held) { return held.key == key; });
    if (found == flashHeld_.end()) {
        return {};  // kein gehaltener Flash (umschaltend, oder das Flash wurde schon beendet)
    }
    const bool wasShown = std::next(found) == flashHeld_.end();
    const double back = found->back;
    flashHeld_.erase(found);
    if (!wasShown) {
        return {};  // ein darunter gehaltener Look: das Bild bleibt beim zuletzt gedrückten
    }
    const int target = flashHeld_.empty() ? flashReturn_ : flashHeld_.back().look;
    if (flashHeld_.empty()) {
        flashReturn_ = -1;
    }
    if (!inRange(target, project_->looks.size())) {
        return {};
    }
    if (const Result<void> armed = armLook(target); !armed.ok()) {
        return armed;
    }
    return goLook(back);
}

Result<void> Player::liveDown(int index, std::optional<double> fadeSeconds, bool useGo) {
    if (!project_ || !live_) {
        return Error{ErrorCode::InvalidArgument, "Der Live-Betrieb ist nicht gestartet"};
    }
    if (!inRange(index, project_->looks.size())) {
        return Error{ErrorCode::NotFound, "Es gibt keinen Look Nummer " + std::to_string(index + 1)};
    }
    const project::Look& look = project_->looks[static_cast<std::size_t>(index)];
    if (look.trigger != project::LookTrigger::Flash) {
        return useGo ? liveArm(index) : liveTrigger(index, fadeSeconds);
    }
    return flashDown(index, index, look.flashInSeconds, look.returnSeconds.value_or(look.fadeSeconds), fadeSeconds);
}

Result<void> Player::liveUp(int index) { return flashUp(index); }

int Player::cellLook(int cell) const {
    if (!project_ || !inRange(cell, project_->liveGrid.cells.size())) {
        return -1;
    }
    const project::LiveCell& entry = project_->liveGrid.cells[static_cast<std::size_t>(cell)];
    return entry.empty() ? -1 : findLook(project_->looks, entry.look);
}

Result<void> Player::liveCellDown(int cell, std::optional<double> fadeSeconds, bool useGo) {
    if (!project_ || !live_) {
        return Error{ErrorCode::InvalidArgument, "Der Live-Betrieb ist nicht gestartet"};
    }
    if (!inRange(cell, project_->liveGrid.cells.size())) {
        return Error{ErrorCode::NotFound, "Es gibt kein Feld Nummer " + std::to_string(cell + 1)};
    }
    const int look = cellLook(cell);
    if (look < 0) {
        return {};  // leeres Feld
    }
    const project::LiveCell& entry = project_->liveGrid.cells[static_cast<std::size_t>(cell)];
    if (entry.trigger != project::LookTrigger::Flash) {
        return useGo ? liveArm(look) : liveTrigger(look, fadeSeconds);
    }
    const double back = entry.returnSeconds.value_or(project_->looks[static_cast<std::size_t>(look)].fadeSeconds);
    return flashDown(-(cell + 1), look, entry.flashInSeconds, back, fadeSeconds);
}

Result<void> Player::liveCellUp(int cell) { return flashUp(-(cell + 1)); }

Result<void> Player::liveCellTrigger(int cell, std::optional<double> fadeSeconds) {
    const int look = cellLook(cell);
    return look < 0 ? Result<void>{} : liveTrigger(look, fadeSeconds);
}

}  // namespace whitehole::runtime
