// live_decks.cpp: Deck-Wechsel und Überblendung.
#include "whitehole/runtime/live_decks.hpp"

#include <algorithm>
#include <cmath>

#include "whitehole/runtime/live_look.hpp"

namespace whitehole::runtime {

namespace {

constexpr const char* kMixerType = "wh.blend.crossfade";

constexpr const char* kFillerType = "wh.generator.solid";

// Modi des Mischers (Reihenfolge in builtin_blends.cpp): Linear für Überblendungen, die beiden Overlay-Modi für ein Flash
// als Overlay - welches Deck oben liegt, hängt davon ab, welches gerade sichtbar ist.
constexpr std::int64_t kModeLinear = 0;
constexpr std::int64_t kModeOverlayBOverA = 2;
constexpr std::int64_t kModeOverlayAOverB = 3;

const char* inputOf(bool deckIsA) { return deckIsA ? "a" : "b"; }

}  // namespace

LiveDecks::LiveDecks(std::shared_ptr<const graph::INodeRegistry> registry) : graph_{std::move(registry)} {}

Result<void> LiveDecks::start(const project::Look& look, int index, int cell) {
    // Von vorn: ein neuer Graph mit dem Mischer als Ausgang
    graph_ = graph::Graph{graph_.registryPtr()};
    a_ = Slot{};
    b_ = Slot{};
    auto mixer = graph_.addNode(kMixerType, Vec2{});
    if (!mixer.ok()) {
        return mixer.error();
    }
    mixer_ = mixer.value();
    target_ = graph::PortRef{mixer_, "out"};
    visible_ = Deck::A;
    armed_ = false;
    fading_ = false;
    progress_ = 1.0;
    amount_ = 0.0;
    overlay_ = false;
    overlayCarry_.reset();
    fill(Deck::B);
    if (const Result<void> loaded = load(Deck::A, look, index, cell); !loaded.ok()) {
        return loaded;
    }
    setAmount(0.0);
    structureChanged_ = true;
    return {};
}

void LiveDecks::clear(Deck deck) {
    Slot& s = slot(deck);
    for (const NodeId id : s.nodes) {
        (void)graph_.removeNode(id);
    }
    s = Slot{};
}

// Ein leeres Deck bekommt eine durchsichtige Fläche, denn der Mischer verlangt an beiden Eingängen etwas.
void LiveDecks::fill(Deck deck) {
    slot(deck) = Slot{};
    const auto filler = graph_.addNode(kFillerType, Vec2{});
    if (!filler.ok()) {
        return;
    }
    (void)graph_.setParameter(filler.value(), "color", ColorRgba{0.0F, 0.0F, 0.0F, 0.0F});
    (void)graph_.connect(graph::PortRef{filler.value(), "out"}, graph::PortRef{mixer_, inputOf(deck == Deck::A)});
    slot(deck).nodes.push_back(filler.value());
    structureChanged_ = true;
}

Result<void> LiveDecks::load(Deck deck, const project::Look& look, int index, int cell) {
    clear(deck);
    auto placed = placeLook(graph_, look, Vec2{});
    if (!placed.ok()) {
        // Halb eingesetzte Nodes wieder entfernen: alle, die nicht zu den bekannten gehören
        std::vector<NodeId> known{mixer_};
        for (const NodeId id : slot(other(deck)).nodes) {
            known.push_back(id);
        }
        std::vector<NodeId> stray;
        for (const graph::NodeInstance& node : graph_.nodes()) {
            if (std::ranges::find(known, node.id) == known.end()) {
                stray.push_back(node.id);
            }
        }
        for (const NodeId id : stray) {
            (void)graph_.removeNode(id);
        }
        fill(deck);
        return placed.error();
    }
    Slot& s = slot(deck);
    s.nodes = std::move(placed.value().nodes);
    s.look = index;
    s.cell = cell;
    const Result<ConnectionId> connected =
        graph_.connect(placed.value().output, graph::PortRef{mixer_, inputOf(deck == Deck::A)});
    if (!connected.ok()) {
        clear(deck);
        fill(deck);
        return connected.error();
    }
    structureChanged_ = true;
    return {};
}

void LiveDecks::finishFade() {
    fading_ = false;
    manual_ = false;
    progress_ = 1.0;
    visible_ = other(visible_);
    armed_ = false;
    setAmount(visible_ == Deck::A ? 0.0 : 1.0);
}

Result<void> LiveDecks::arm(const project::Look& look, int index, int cell) {
    overlayCarry_.reset();
    if (overlay_) {
        overlayCarry_ = overlayLevel_;
        endOverlay();
    }
    if (fading_) {
        finishFade();
    }
    const Deck hidden = other(visible_);
    armed_ = false;
    if (const Result<void> loaded = load(hidden, look, index, cell); !loaded.ok()) {
        return loaded;
    }
    armed_ = true;
    return {};
}

Result<void> LiveDecks::armBlank() {
    endOverlay();
    overlayCarry_.reset();
    if (fading_) {
        finishFade();
    }
    const Deck hidden = other(visible_);
    clear(hidden);
    fill(hidden);
    armed_ = true;
    return {};
}

Result<void> LiveDecks::go(double fadeSeconds) {
    if (!armed_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Look vorgeladen"};
    }
    if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0) {
        return Error{ErrorCode::InvalidArgument, "Die Überblendzeit ist ungültig"};
    }
    overlayCarry_.reset();
    fadeSeconds_ = fadeSeconds;
    if (fading_ && manual_) {
        manual_ = false;  // von der erreichten Stelle aus mit der Uhr weiter (advance)
    } else {
        fading_ = true;
        progress_ = 0.0;
    }
    if (fadeSeconds <= 0.0) {
        finishFade();
    }
    return {};
}

bool LiveDecks::advance(double seconds) {
    if (overlay_) {
        return advanceOverlay(seconds);
    }
    if (!fading_ || manual_) {
        return false;
    }
    if (std::isfinite(seconds) && seconds > 0.0) {
        progress_ = std::min(1.0, progress_ + seconds / fadeSeconds_);
    }
    if (progress_ >= 1.0) {
        finishFade();
        return true;
    }
    showProgress();
    return true;
}

void LiveDecks::showProgress() {
    // Von der Position des sichtbaren Decks zur anderen: A -> B steigt von 0 auf 1, B -> A fällt
    const double from = visible_ == Deck::A ? 0.0 : 1.0;
    setAmount(from + (1.0 - 2.0 * from) * progress_);
}

bool LiveDecks::setManualProgress(double progress) {
    if (!armed_ || (fading_ && !manual_) || !std::isfinite(progress)) {
        return false;
    }
    progress = std::clamp(progress, 0.0, 1.0);
    if (progress >= 1.0) {
        finishFade();
        return true;
    }
    if (progress <= 0.0) {
        if (!fading_) {
            return false;
        }
        fading_ = false;  // zurück auf "nur vorgeladen"
        manual_ = false;
        progress_ = 1.0;
        setAmount(visible_ == Deck::A ? 0.0 : 1.0);
        return true;
    }
    fading_ = true;
    manual_ = true;
    fadeSeconds_ = 0.0;
    progress_ = progress;
    showProgress();
    return true;
}

void LiveDecks::setAmount(double amount) {
    amount_ = std::clamp(amount, 0.0, 1.0);
    (void)graph_.setParameter(mixer_, "amount", amount_);
}

Result<void> LiveDecks::goOverlay(double fadeSeconds) {
    if (!armed_ || fading_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Look vorgeladen"};
    }
    if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0) {
        return Error{ErrorCode::InvalidArgument, "Die Überblendzeit ist ungültig"};
    }
    armed_ = false;
    overlay_ = true;
    overlayLevel_ = overlayCarry_.value_or(0.0);
    overlayCarry_.reset();
    overlayTarget_ = 1.0;
    overlaySeconds_ = fadeSeconds;
    if (fadeSeconds <= 0.0) {
        overlayLevel_ = 1.0;
    }
    setMode(visible_ == Deck::A ? kModeOverlayBOverA : kModeOverlayAOverB);
    setAmount(overlayLevel_);
    return {};
}

bool LiveDecks::releaseOverlay(double fadeSeconds) {
    if (!overlay_) {
        return false;
    }
    overlayTarget_ = 0.0;
    overlaySeconds_ = std::isfinite(fadeSeconds) && fadeSeconds > 0.0 ? fadeSeconds : 0.0;
    if (overlaySeconds_ <= 0.0 || overlayLevel_ <= 0.0) {  // sofort, oder noch gar nicht eingeblendet
        endOverlay();
    }
    return true;
}

void LiveDecks::endOverlay() noexcept {
    if (!overlay_) {
        return;
    }
    overlay_ = false;
    overlayLevel_ = 0.0;
    overlayTarget_ = 0.0;
    setMode(kModeLinear);
    setAmount(visible_ == Deck::A ? 0.0 : 1.0);
}

bool LiveDecks::advanceOverlay(double seconds) {
    if (overlayLevel_ == overlayTarget_) {
        return false;
    }
    const double step = overlaySeconds_ > 0.0 ? (std::isfinite(seconds) && seconds > 0.0 ? seconds / overlaySeconds_ : 0.0) : 1.0;
    overlayLevel_ = overlayTarget_ > overlayLevel_ ? std::min(overlayTarget_, overlayLevel_ + step)
                                                   : std::max(overlayTarget_, overlayLevel_ - step);
    if (overlayLevel_ <= 0.0 && overlayTarget_ <= 0.0) {
        endOverlay();
        return true;
    }
    setAmount(overlayLevel_);
    return true;
}

void LiveDecks::setMode(std::int64_t mode) noexcept {
    (void)graph_.setParameter(mixer_, "mode", graph::ParameterValue{mode});
}

bool LiveDecks::takeStructureChanged() noexcept {
    const bool changed = structureChanged_;
    structureChanged_ = false;
    return changed;
}

LiveStatus LiveDecks::status() const noexcept {
    LiveStatus s;
    const Slot& shown = visible_ == Deck::A ? a_ : b_;
    const Slot& hidden = visible_ == Deck::A ? b_ : a_;
    s.current = shown.look;
    s.armed = armed_ && !fading_ ? hidden.look : -1;
    s.fading = fading_;
    s.incoming = fading_ ? hidden.look : -1;
    s.progress = progress_;
    s.fadeSeconds = fadeSeconds_;
    s.manual = fading_ && manual_;
    s.currentCell = shown.cell;
    s.armedCell = s.armed >= 0 ? hidden.cell : -1;
    s.incomingCell = fading_ ? hidden.cell : -1;
    s.overlay = overlay_;
    s.overlayLook = overlay_ ? hidden.look : -1;
    s.overlayCell = overlay_ ? hidden.cell : -1;
    s.overlayLevel = overlay_ ? overlayLevel_ : 0.0;
    return s;
}

std::vector<graph::PortRef> LiveDecks::deckOutputs() const {
    std::vector<graph::PortRef> outputs;
    for (const bool deckIsA : {true, false}) {
        for (const graph::Connection& connection : graph_.connections()) {
            if (connection.to.node == mixer_ && connection.to.portKey == inputOf(deckIsA)) {
                outputs.push_back(connection.from);
            }
        }
    }
    if (outputs.size() != 2) {
        return {};  // vor start() (oder ein Deck ohne Eingang): keine Vorschau
    }
    return outputs;
}

std::optional<std::uint32_t> LiveDecks::previewDeck() const noexcept {
    if (!armed_ && !fading_ && !overlay_) {
        return std::nullopt;
    }
    return other(visible_) == Deck::A ? 0U : 1U;
}

}  // namespace whitehole::runtime
