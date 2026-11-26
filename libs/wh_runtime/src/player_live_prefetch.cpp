// player_live_prefetch.cpp: im Live-Betrieb die Felder des laufenden Looks vorab übersetzen (Teil des Players).
//
// Warum es diese Datei gibt: Das Live-Bild ist ein Shader aus beiden Decks (live_decks.hpp). Jeder Look, der ins
// verdeckte Deck kommt, ergibt eine neue Kombination, die der Treiber erst übersetzen muss (auf der Grafikkarte des
// Nutzers etwa 0,3 s). Beim Vorladen fällt das kaum auf, ein Flash soll aber sofort sitzen (Rückmeldung vom 26.11.: das
// erste Drücken eines Flash-Looks stockte). Darum bestellt der Player, sobald ein Look steht, für jedes belegte Feld des
// Live-Rasters die Kombination "laufender Look + dieser Look im verdeckten Deck" vorab (Session::prefetchGraph):
// Flash-Felder zuerst, dann die übrigen, höchstens eines je Frame (das Übersetzen in WGSL kostet im Hauptablauf etwa
// 1 ms). Der Treiber arbeitet die Aufträge nachrangig ab; wird ein Feld gedrückt, bevor seine Kombination fertig ist,
// zieht der FieldPass den Auftrag vor. Wechselt der sichtbare Look, verfallen die offenen Aufträge.
//
// Nicht während einer Überblendung, solange ein neuer Shader noch wartet oder ein Flash gehalten wird: Dann steht noch
// nicht fest, welcher Look die Basis für das nächste Drücken ist.
#include <algorithm>

#include "whitehole/runtime/live_look.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

/// So viele Looks werden je laufendem Look höchstens vorab übersetzt (der Zwischenspeicher im FieldPass fasst 48).
constexpr std::size_t kMaxPrefetchLooks = 32;

}  // namespace

std::vector<int> Player::livePrefetchLooks() const {
    std::vector<int> looks;
    if (!project_) {
        return looks;
    }
    const auto collect = [&](bool flash) {
        for (const project::LiveCell& cell : project_->liveGrid.cells) {
            if (cell.empty() || (cell.trigger == project::LookTrigger::Flash) != flash) {
                continue;
            }
            const int look = findLook(project_->looks, cell.look);
            if (look >= 0 && std::ranges::find(looks, look) == looks.end() && looks.size() < kMaxPrefetchLooks) {
                looks.push_back(look);
            }
        }
    };
    collect(true);   // Flash-Felder zuerst: die sollen sofort sitzen
    collect(false);  // dann die übrigen (Abrufen ohne Vorladen)
    return looks;
}

void Player::prefetchLive() {
    if (!live_ || !session_ || !project_ || !flashHeld_.empty()) {
        return;
    }
    const LiveStatus status = live_->status();
    if (status.fading || session_->graphPending()) {
        return;
    }
    std::vector<int> wanted = livePrefetchLooks();
    const bool deckA = live_->showsDeckA();
    if (status.current != livePrefetch_.look || deckA != livePrefetch_.deckA || wanted != livePrefetch_.wanted) {
        session_->cancelPrefetches();  // für einen anderen Look bestellt: nicht mehr gebraucht
        livePrefetch_ = LivePrefetch{status.current, deckA, std::move(wanted), 0};
    }
    if (livePrefetch_.next >= livePrefetch_.wanted.size()) {
        return;
    }
    const int look = livePrefetch_.wanted[livePrefetch_.next++];
    // Der Graph, der entstünde, käme dieser Look jetzt ins verdeckte Deck (die echten Decks bleiben unberührt)
    LiveDecks preview = *live_;
    if (!preview.arm(project_->looks[static_cast<std::size_t>(look)], look, -1).ok()) {
        return;
    }
    const std::vector<graph::PortRef> extras = splitPreview_ ? preview.deckOutputs() : std::vector<graph::PortRef>{};
    (void)session_->prefetchGraph(preview.graph(), extras);  // ein Fehler hier meldet sich, falls der Look wirklich kommt
}

}  // namespace whitehole::runtime
