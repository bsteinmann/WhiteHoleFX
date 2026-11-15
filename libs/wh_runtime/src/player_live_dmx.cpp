// player_live_dmx.cpp: Look-Auswahl im Live-Betrieb per DMX-Kanal (siehe live_dmx.hpp).
//
// Warum eine eigene Datei: wie beim Master (player_master.cpp) ein abgeschlossenes Stück Pult-Verhalten; läuft
// einmal je step(), vor dem Bild. Anders als der Master (der jeden Frame stetig dem Fader folgt) ist die
// Look-Auswahl ein Umschalter: ausgelöst wird nur bei einem Wechsel des berechneten Look-Index, nicht auf jedem
// Frame erneut (sonst würde eine laufende Überblendung durch denselben, weiterhin hohen Kanalwert sofort wieder
// von vorn beginnen).
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

void Player::setLiveDmx(std::optional<LiveDmxSource> source) {
    liveDmx_ = source;
    liveDmxLive_ = false;
    liveDmxLastIndex_.reset();
}

void Player::pollLiveDmx() {
    liveDmxLive_ = false;
    if (!liveDmx_ || !library_.dmx || !live_ || !project_ || project_->looks.empty()) {
        return;
    }
    const std::optional<input::DmxSample> selectSample =
        library_.dmx->read(liveDmx_->protocol, liveDmx_->universe, liveDmx_->selectChannel, 1);
    if (!selectSample || selectSample->ageSeconds > kLiveDmxTimeoutSeconds) {
        return;  // kein frisches Signal: die Auswahl hält ihren Stand
    }
    liveDmxLive_ = true;
    const std::optional<int> index = lookIndexFromDmxValue(selectSample->values[0], project_->looks.size());
    if (!index || index == liveDmxLastIndex_) {
        return;  // 0 (kein Wechsel) oder derselbe Look wie zuletzt: nicht erneut auslösen
    }
    liveDmxLastIndex_ = index;
    std::optional<double> fadeSeconds;
    if (liveDmx_->fadeChannel >= 1) {
        const std::optional<input::DmxSample> fadeSample =
            library_.dmx->read(liveDmx_->protocol, liveDmx_->universe, liveDmx_->fadeChannel, 1);
        if (fadeSample && fadeSample->ageSeconds <= kLiveDmxTimeoutSeconds) {
            fadeSeconds = static_cast<double>(fadeSample->values[0]) / 255.0 * kLiveDmxMaxFadeSeconds;
        }
    }
    (void)liveTrigger(*index, fadeSeconds);
}

}  // namespace whitehole::runtime
