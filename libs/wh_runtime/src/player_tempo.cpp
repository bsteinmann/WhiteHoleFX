// player_tempo.cpp: die gemeinsame Tempo-Uhr am Player (Teil des Players).
//
// Warum es diese Datei gibt: Die Tempo-Uhr (values::TempoClock) liegt in der BuiltinLibrary und überlebt Look- und
// Projektwechsel. Weiterschieben muss sie einer, der jedes Bild läuft und eine Uhr hat, die sich in Tests vorgeben lässt:
// der Player mit seiner Live-Uhr (liveNow, setLiveClock). Hier stehen die kleinen Zugänge dafür; step() ruft advanceTempo().
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

values::TempoClock::Snapshot Player::tempo() const {
    return library_.tempo ? library_.tempo->snapshot() : values::TempoClock::Snapshot{};
}

void Player::advanceTempo() {
    if (library_.tempo) {
        library_.tempo->update(liveNow());  // vor dem Rechnen der Nodes: alle sehen im Bild denselben Stand
    }
}

}  // namespace whitehole::runtime
