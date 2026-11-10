// player_signals.cpp: Live-Werte der CPU-Nodes für den Editor (Teil des Players, ausgelagert wegen der Dateigröße).
//
// Warum es das gibt: Der Editor soll zeigen, was an den Ausgängen gerade anliegt. Die Werte entstehen im Frame
// (Session -> Abtast-Stage -> ValueStore); hier wird nur gelesen, nie geschrieben.
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

std::vector<LiveSignal> Player::liveSignals() const {
    if (!session_) {
        return {};
    }
    return collectLiveSignals(session_->values());
}

}  // namespace whitehole::runtime
