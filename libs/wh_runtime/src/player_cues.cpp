// player_cues.cpp: Cuelist im Player (Issue #21): Cues setzen, starten, auslösen und je Frame weiterschalten.
//
// Warum eine eigene Datei: Player ist nach Themen auf mehrere Quelldateien verteilt (Dateigröße). Die Logik, welcher
// Cue wann dran ist, steht in cue_runner.cpp; hier wird sie an die Live-Decks, den DMX-Speicher und den Audio-Speicher
// gehängt.
#include <cmath>

#include "whitehole/runtime/live_look.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidArgument, std::move(message)}; }

Result<void> validate(const std::vector<project::Cue>& cues, const std::vector<project::Look>& looks) {
    for (std::size_t i = 0; i < cues.size(); ++i) {
        const project::Cue& cue = cues[i];
        const std::string where = "Cue " + std::to_string(i + 1) + ": ";
        for (const double seconds : {cue.delay, cue.fade, cue.time}) {
            if (!std::isfinite(seconds) || seconds < 0.0 || seconds > project::kMaxCueSeconds) {
                return invalid(where + "Zeiten müssen zwischen 0 und 3600 Sekunden liegen");
            }
        }
        if (!cue.look.empty() && findLook(looks, cue.look) < 0) {
            return Error{ErrorCode::NotFound, where + "Der Look \"" + cue.look + "\" gibt es nicht"};
        }
        if (cue.trigger == project::CueTrigger::Key && cue.key.empty()) {
            return invalid(where + "Dem Tasten-Auslöser fehlt die Taste");
        }
        if (cue.trigger == project::CueTrigger::Dmx &&
            (cue.dmxChannel < 1 || cue.dmxChannel > 512 || cue.dmxThreshold < 1 || cue.dmxThreshold > 255 ||
             cue.dmxUniverse > 32767)) {
            return invalid(where + "DMX-Kanal (1 bis 512), Schwelle (1 bis 255) oder Universe stimmen nicht");
        }
    }
    return {};
}

}  // namespace

const std::vector<project::Cue>& Player::cues() const {
    static const std::vector<project::Cue> kNone;
    return project_ ? project_->cues : kNone;
}

Result<void> Player::setCues(std::vector<project::Cue> cues) {
    if (!project_) {
        return invalid("Es ist kein Projekt geladen");
    }
    if (const Result<void> ok = validate(cues, project_->looks); !ok.ok()) {
        return ok;
    }
    cueRunner_.stop();
    project_->cues = std::move(cues);
    modified_ = true;
    return {};
}

Result<void> Player::cuesStart() {
    if (!project_) {
        return invalid("Es ist kein Projekt geladen");
    }
    if (project_->cues.empty()) {
        return invalid("Die Cuelist ist leer");
    }
    if (project_->looks.empty()) {
        return invalid("Es sind noch keine Looks gespeichert");
    }
    if (const Result<void> ok = validate(project_->cues, project_->looks); !ok.ok()) {
        return ok;
    }
    const project::Cue& first = project_->cues.front();
    const int firstLook = first.look.empty() ? 0 : findLook(project_->looks, first.look);
    if (const Result<void> started = startLive(firstLook); !started.ok()) {
        return started;
    }
    cueError_.reset();
    if (first.look.empty()) {
        // Der erste Cue ist Schwarz: sofort ausblenden
        (void)live_->armBlank();
        (void)live_->go(0.0);
    }
    cueBeats_ = 0;
    if (library_.audio) {
        if (const auto snapshot = library_.audio->read()) {
            cueBeats_ = snapshot->frame.beat.count;
        }
    }
    cueRunner_.load(project_->cues);
    cueRunner_.start(liveNow());
    return {};
}

void Player::cuesStop() { cueRunner_.stop(); }

Result<void> Player::cueFire(int index) {
    if (!cueRunner_.running()) {
        return invalid("Die Cuelist läuft nicht");
    }
    if (!cueRunner_.fire(index, liveNow())) {
        return Error{ErrorCode::NotFound, "Es gibt keinen Cue Nummer " + std::to_string(index + 1)};
    }
    return {};
}

Result<void> Player::executeCue(int index) {
    const project::Cue& cue = project_->cues[static_cast<std::size_t>(index)];
    if (cue.look.empty()) {
        if (const Result<void> armed = live_->armBlank(); !armed.ok()) {
            return armed;
        }
    } else {
        const int look = findLook(project_->looks, cue.look);
        if (look < 0) {
            return Error{ErrorCode::NotFound, "Der Look \"" + cue.look + "\" gibt es nicht"};
        }
        if (const Result<void> armed = live_->arm(project_->looks[static_cast<std::size_t>(look)], look); !armed.ok()) {
            return armed;
        }
    }
    const Result<void> go = live_->go(cue.fade);
    if (go.ok()) {
        liveLast_.reset();  // die Überblendung beginnt mit dem nächsten Frame
    }
    return go;
}

void Player::advanceCues() {
    if (!cueRunner_.running()) {
        return;
    }
    if (!live_ || !project_) {
        cueRunner_.stop();
        return;
    }
    CueEvents events;
    events.go = cueGo_;
    cueGo_ = false;
    events.keys = std::move(cueKeys_);
    cueKeys_.clear();
    if (library_.audio) {
        if (const auto snapshot = library_.audio->read()) {
            const std::uint64_t beats = snapshot->frame.beat.count;
            events.beat = beats > cueBeats_;
            cueBeats_ = beats;
        }
    }
    events.dmx = [this](const project::Cue& cue) -> std::optional<int> {
        if (!library_.dmx) {
            return std::nullopt;
        }
        const input::DmxProtocol protocol =
            cue.dmxProtocol == project::CueDmxProtocol::Sacn ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
        const auto sample = library_.dmx->read(protocol, cue.dmxUniverse, cue.dmxChannel, 1);
        if (!sample || sample->ageSeconds > kMasterDmxTimeoutSeconds) {
            return std::nullopt;
        }
        return static_cast<int>(sample->values[0]);
    };
    if (const std::optional<int> due = cueRunner_.update(liveNow(), events)) {
        if (const Result<void> executed = executeCue(*due); !executed.ok()) {
            cueError_ = executed.error();
        }
    }
}

}  // namespace whitehole::runtime
