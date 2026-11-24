// player_live_preview.cpp: geteilte Vorschau im Live-Betrieb (Teil des Players).
//
// Warum es diese Datei gibt: Neben der Ausgabe soll die Oberfläche den vorgeladenen Look laufen lassen. Der steckt schon
// im Live-Graphen (verdecktes Deck, siehe live_decks.hpp) und wird vom Crossfade ohnehin mitgerechnet; hier wird er
// zusätzlich einzeln abgetastet (Zusatzausgang der Session, siehe session.hpp) und als zweites Bild geliefert. Ausgelagert
// wegen der Dateigröße von player.cpp.
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

void Player::setSplitPreview(bool on) {
    if (splitPreview_ == on) {
        return;
    }
    splitPreview_ = on;
    if (live_ && session_) {
        rebuildDirty_ = true;  // der Platz für das zweite Bild steckt im Farbpuffer der Session
    }
}

std::vector<graph::PortRef> Player::previewTargets() const {
    if (!live_ || !splitPreview_) {
        return {};
    }
    return live_->deckOutputs();
}

void Player::selectPreview(bool reloaded) {
    if (!session_) {
        return;
    }
    const std::optional<std::uint32_t> deck = live_ && splitPreview_ ? live_->previewDeck() : std::nullopt;
    // Nach dem Laden eines Looks (neuer Shader) gilt auch beim selben Deck eine neue Kennung: ältere Readbacks
    // zeigen sonst noch kurz den vorherigen Look.
    if (!reloaded && previewSelected_ && *previewSelected_ == deck) {
        return;
    }
    if (const Result<void> selected = session_->selectSecondary(deck); selected.ok()) {
        previewSelected_ = deck;
    }
}

std::optional<int> Player::previewLook() const noexcept {
    if (!live_ || !splitPreview_ || !live_->previewDeck()) {
        return std::nullopt;
    }
    const LiveStatus status = live_->status();
    return status.fading ? status.incoming : status.armed;
}

Result<preview::Image> Player::previewImage(const preview::PreviewOptions& given) const {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    std::vector<ColorRgba> colors = session_ && previewLook() ? session_->latestSecondaryColors() : std::vector<ColorRgba>{};
    if (colors.empty()) {
        return Error{ErrorCode::NotFound, "Kein Look vorgeladen"};
    }
    preview::PreviewOptions options = given;
    previewGamma(options);
    return preview::renderPreview(map_, colors, options);
}

Result<preview::Image> Player::previewImage3d(const preview::Camera3d& camera, const preview::PreviewOptions& given) const {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    std::vector<ColorRgba> colors = session_ && previewLook() ? session_->latestSecondaryColors() : std::vector<ColorRgba>{};
    if (colors.empty()) {
        return Error{ErrorCode::NotFound, "Kein Look vorgeladen"};
    }
    preview::PreviewOptions options = given;
    previewGamma(options);
    return preview::renderPreview3d(map_, colors, camera, options);
}

}  // namespace whitehole::runtime
