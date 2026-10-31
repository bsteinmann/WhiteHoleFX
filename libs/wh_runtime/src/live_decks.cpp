// live_decks.cpp: Deck-Wechsel und Überblendung.
#include "whitehole/runtime/live_decks.hpp"

#include <algorithm>
#include <cmath>

#include "whitehole/runtime/live_look.hpp"

namespace whitehole::runtime {

namespace {

constexpr const char* kMixerType = "wh.blend.crossfade";

constexpr const char* kFillerType = "wh.generator.solid";

const char* inputOf(bool deckIsA) { return deckIsA ? "a" : "b"; }

}  // namespace

LiveDecks::LiveDecks(std::shared_ptr<const graph::INodeRegistry> registry) : graph_{std::move(registry)} {}

Result<void> LiveDecks::start(const project::Look& look, int index) {
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
    fill(Deck::B);
    if (const Result<void> loaded = load(Deck::A, look, index); !loaded.ok()) {
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

Result<void> LiveDecks::load(Deck deck, const project::Look& look, int index) {
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
    progress_ = 1.0;
    visible_ = other(visible_);
    armed_ = false;
    setAmount(visible_ == Deck::A ? 0.0 : 1.0);
}

Result<void> LiveDecks::arm(const project::Look& look, int index) {
    if (fading_) {
        finishFade();
    }
    const Deck hidden = other(visible_);
    armed_ = false;
    if (const Result<void> loaded = load(hidden, look, index); !loaded.ok()) {
        return loaded;
    }
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
    fadeSeconds_ = fadeSeconds;
    fading_ = true;
    progress_ = 0.0;
    if (fadeSeconds <= 0.0) {
        finishFade();
    }
    return {};
}

bool LiveDecks::advance(double seconds) {
    if (!fading_) {
        return false;
    }
    if (std::isfinite(seconds) && seconds > 0.0) {
        progress_ = std::min(1.0, progress_ + seconds / fadeSeconds_);
    }
    if (progress_ >= 1.0) {
        finishFade();
        return true;
    }
    // Von der Position des sichtbaren Decks zur anderen: A -> B steigt von 0 auf 1, B -> A fällt
    const double from = visible_ == Deck::A ? 0.0 : 1.0;
    setAmount(from + (1.0 - 2.0 * from) * progress_);
    return true;
}

void LiveDecks::setAmount(double amount) {
    amount_ = std::clamp(amount, 0.0, 1.0);
    (void)graph_.setParameter(mixer_, "amount", amount_);
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
    return s;
}

}  // namespace whitehole::runtime
