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
        std::erase(flashHeld_, index);  // ein gehaltener Look hört auf zu flashen; das Bild bleibt, wie es ist
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
    if (flashHeld_.empty()) {
        const LiveStatus status = live_->status();
        flashReturn_ = status.fading ? status.incoming : status.current;  // was gerade zu sehen ist bzw. kommt
    }
    std::erase(flashHeld_, index);
    flashHeld_.push_back(index);
    if (const Result<void> armed = armLook(index); !armed.ok()) {
        return armed;
    }
    // Die eigene Einblendzeit des Looks gilt vor der Zeit des Live-Fensters und der des Looks
    return goLook(look.flashInSeconds ? std::optional<double>{*look.flashInSeconds} : fadeSeconds);
}

Result<void> Player::liveUp(int index) {
    if (!project_ || !live_) {
        return {};
    }
    const auto found = std::find(flashHeld_.begin(), flashHeld_.end(), index);
    if (found == flashHeld_.end()) {
        return {};  // kein gehaltener Flash-Look (umschaltend, oder das Flash wurde schon beendet)
    }
    const bool wasShown = std::next(found) == flashHeld_.end();
    flashHeld_.erase(found);
    if (!wasShown) {
        return {};  // ein darunter gehaltener Look: das Bild bleibt beim zuletzt gedrückten
    }
    const project::Look& released = project_->looks[static_cast<std::size_t>(index)];
    const double back = released.returnSeconds.value_or(released.fadeSeconds);
    const int target = flashHeld_.empty() ? flashReturn_ : flashHeld_.back();
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

}  // namespace whitehole::runtime
