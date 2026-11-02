// player_master.cpp: Master-Helligkeit aus einem DMX-Kanal (Issue #17).
//
// Warum eine eigene Datei: player.cpp ist ohnehin voll; das Pult-Verhalten (lesen, Zeitüberschreitung, Wert
// übernehmen) ist ein abgeschlossenes Stück. Es läuft einmal je step(), vor dem Bild.
#include <cmath>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

void Player::setMasterDmx(std::optional<MasterDmxSource> source) {
    if (source && (source->channel < 1 || source->channel > input::kDmxChannels)) {
        source.reset();  // ein Kanal außerhalb von 1..512 liest nie etwas
    }
    masterDmx_ = source;
    masterDmxLive_ = false;
    pollMasterDmx();
}

void Player::pollMasterDmx() {
    masterDmxLive_ = false;
    if (!masterDmx_ || !library_.dmx) {
        return;
    }
    const std::optional<input::DmxSample> sample =
        library_.dmx->read(masterDmx_->protocol, masterDmx_->universe, masterDmx_->channel, 1);
    if (!sample || sample->ageSeconds > kMasterDmxTimeoutSeconds) {
        return;  // kein frisches Signal: der Master hält seinen Wert
    }
    masterDmxLive_ = true;
    const float level = static_cast<float>(sample->values[0]) / 255.0F;
    if (std::abs(level - master_) > 1e-6F) {
        master_ = level;
        applyOutputLevel();
    }
}

}  // namespace whitehole::runtime
