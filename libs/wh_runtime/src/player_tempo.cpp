// player_tempo.cpp: die gemeinsame Tempo-Uhr am Player (Teil des Players).
//
// Warum es diese Datei gibt: Die Tempo-Uhr (values::TempoClock) liegt in der BuiltinLibrary und überlebt Look- und
// Projektwechsel. Weiterschieben muss sie einer, der jedes Bild läuft und eine Uhr hat, die sich in Tests vorgeben lässt:
// der Player mit seiner Live-Uhr (liveNow, setLiveClock). Hier stehen die kleinen Zugänge dafür; step() ruft advanceTempo().
// Für die Quellen Audio und DMX liest er außerdem den Beat-Erkenner (AudioStore) bzw. den DMX-Kanal und meldet sie der Uhr,
// die selbst weder Audio noch DMX kennt.
#include <cmath>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

void Player::tempoTap() {
    if (library_.tempo) {
        library_.tempo->tap();
    }
}

void Player::setTempoSource(values::TempoClock::Source source) {
    if (library_.tempo) {
        library_.tempo->setSource(source);
    }
}

void Player::setTempoFixedBpm(double bpm) {
    if (library_.tempo) {
        library_.tempo->setFixedBpm(bpm);
    }
}

void Player::setTempoDmx(std::optional<input::DmxAddress> address) { tempoDmx_ = address; }

values::TempoClock::Snapshot Player::tempo() const {
    return library_.tempo ? library_.tempo->snapshot() : values::TempoClock::Snapshot{};
}

namespace {

constexpr double kAudioTimeout = 0.5;  // Sekunden ohne Audio = kein Signal (wie die Audio-Nodes)
constexpr double kDmxTimeout = 2.0;    // Sekunden ohne DMX = kein Signal (wie die Live-Steuerung)

}  // namespace

void Player::advanceTempo() {
    if (!library_.tempo) {
        return;
    }
    values::TempoClock& clock = *library_.tempo;
    const values::TempoClock::Source source = clock.source();
    if (source == values::TempoClock::Source::Audio && library_.audio) {
        const auto snapshot = library_.audio->read();
        if (snapshot && snapshot->ageSeconds <= kAudioTimeout) {
            const audio::BeatInfo& beat = snapshot->frame.beat;
            if (!tempoAudioSeen_ || beat.count < tempoAudioCount_) {
                tempoAudioSeen_ = true;  // erster Blick (oder Quelle neu gestartet): nichts nachholen
            } else if (beat.count > tempoAudioCount_) {
                clock.externalBeat(beat.bpm);
            }
            tempoAudioCount_ = beat.count;
        } else {
            tempoAudioSeen_ = false;  // kein Signal: die Uhr läuft mit dem letzten Tempo weiter
        }
    } else {
        tempoAudioSeen_ = false;
    }
    if (source == values::TempoClock::Source::Dmx && tempoDmx_ && library_.dmx) {
        const auto sample = library_.dmx->read(tempoDmx_->protocol, tempoDmx_->universe, tempoDmx_->channel, 1);
        if (sample && sample->ageSeconds <= kDmxTimeout && sample->values[0] > 0) {
            clock.externalBpm(static_cast<double>(sample->values[0]));
        }
    }
    clock.update(liveNow());  // vor dem Rechnen der Nodes: alle sehen im Bild denselben Stand
}

}  // namespace whitehole::runtime
