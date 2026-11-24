// player_live_dmx.cpp: Look-Auswahl im Live-Betrieb per DMX-Kanal (siehe live_dmx.hpp).
//
// Warum eine eigene Datei: wie beim Master (player_master.cpp) ein abgeschlossenes Stück Pult-Verhalten; läuft
// einmal je step(), vor dem Bild. Anders als der Master (der jeden Frame stetig dem Fader folgt) ist die
// Look-Auswahl ein Umschalter: ausgelöst wird nur bei einem Wechsel des berechneten Look-Index, nicht auf jedem
// Frame erneut (sonst würde eine laufende Überblendung durch denselben, weiterhin hohen Kanalwert sofort wieder
// von vorn beginnen).
//
// Läuft die Cuelist (Cue-Sprung per DMX), wählt derselbe Kanal einen Cue statt eines Looks: Die Looks stehen dann im
// Cue-Ablauf (Delay, Überblendzeit, Auslöser), und ein direkter Look-Wechsel würde ihn umgehen. Die Stufen verteilen
// sich über die Zahl der Cues; der Fade-Kanal gilt dann nicht, jeder Cue hat seine eigene Zeit.
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

void Player::setLiveDmx(std::optional<LiveDmxSource> source) {
    liveDmx_ = source;
    liveDmxLive_ = false;
    liveDmxLastIndex_.reset();
}

void Player::pollLiveDmx() {
    liveDmxLive_ = false;
    const bool cueMode = cueRunner_.running();
    const bool modeChanged = cueMode != liveDmxCueMode_;  // erst nach einem frischen Wert als gesehen vermerkt
    if (!liveDmx_ || !library_.dmx || !live_ || !project_ || (cueMode ? project_->cues.empty() : project_->looks.empty())) {
        return;
    }
    const std::optional<input::DmxSample> selectSample =
        library_.dmx->read(liveDmx_->protocol, liveDmx_->universe, liveDmx_->selectChannel, 1);
    if (!selectSample || selectSample->ageSeconds > kLiveDmxTimeoutSeconds) {
        return;  // kein frisches Signal: die Auswahl hält ihren Stand
    }
    liveDmxLive_ = true;
    liveDmxCueMode_ = cueMode;
    const std::size_t count = cueMode ? project_->cues.size() : project_->looks.size();
    const std::optional<int> index = lookIndexFromDmxValue(selectSample->values[0], count);
    if (modeChanged) {
        // Anderer Bezug (Cues statt Looks): der Stand des Kanals beim Wechsel gilt als gesehen, ein schon hoher Regler
        // springt nicht beim Start der Cuelist zu einem Cue (wie beim ersten gelesenen Wert der Steuerfunktionen)
        liveDmxLastIndex_ = index;
        return;
    }
    if (!index || index == liveDmxLastIndex_) {
        return;  // 0 (kein Wechsel) oder derselbe Look/Cue wie zuletzt: nicht erneut auslösen
    }
    liveDmxLastIndex_ = index;
    if (cueMode) {
        if (const Result<void> fired = cueFire(*index); !fired.ok()) {
            cueError_ = fired.error();
        }
        return;
    }
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
