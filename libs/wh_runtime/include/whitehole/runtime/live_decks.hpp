// live_decks.hpp: zwei Decks und eine Überblendung, damit ein Look in den nächsten übergehen kann.
//
// Warum es diese Datei gibt: Der Renderer rechnet einen einzigen Field-Graphen. Um von einem Effekt zum
// nächsten weich zu wechseln, müssen beide gleichzeitig laufen und gemischt werden. Dafür baut dieses
// Objekt einen eigenen Live-Graphen:
//
//     Deck A (Nodes von Look 1) ──┐
//                                 ├─ Crossfade ── Ausgang
//     Deck B (Nodes von Look 2) ──┘
//
// Ablauf wie am Lichtpult:
//   1. arm():  der nächste Look wird in das verdeckte Deck geladen (hier entsteht, wenn sich die Struktur
//              ändert, eine neue Shader-Pipeline - ein kurzer Ruckler, deshalb am besten vorher);
//   2. go():   die Überblendung läuft über die Fade-Zeit; dabei ändert sich nur ein Zahlenwert, es wird
//              nichts neu übersetzt;
//   3. advance() wird pro Frame mit der vergangenen Zeit aufgerufen und schiebt die Überblendung weiter.
// Nach dem Fade ist das neue Deck das sichtbare und das alte frei für den nächsten arm().
//
// Reine Logik auf einem Graphen: ohne GPU, ohne Uhr (die Zeit kommt als Parameter), daher genau testbar.
#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

struct LiveStatus {
    int current{-1};     // Look, der zu sehen ist (während eines Fades: der alte); -1 = keiner
    int armed{-1};       // vorgeladen und bereit für go(); -1 = keiner
    bool fading{false};
    int incoming{-1};    // während eines Fades: der neue Look
    double progress{1.0};  // 0..1 des laufenden Fades (1 = kein Fade)
    double fadeSeconds{0.0};
};

class LiveDecks {
public:
    explicit LiveDecks(std::shared_ptr<const graph::INodeRegistry> registry);

    /// Baut den Live-Graphen auf und zeigt `look` sofort (Deck A). Fehler: die von placeLook und connect.
    Result<void> start(const project::Look& look, int index);

    /// Lädt `look` in das verdeckte Deck. Läuft gerade ein Fade, wird er zuerst abgeschlossen (die Ansicht
    /// springt dann sofort auf den neuen Look). Fehler: die von placeLook; bei einem Fehler bleibt das
    /// sichtbare Deck unberührt, das verdeckte zeigt einen durchsichtigen Platzhalter.
    Result<void> arm(const project::Look& look, int index);

    /// Startet die Überblendung zum vorgeladenen Look in `fadeSeconds` (0 = sofort).
    /// Fehler: InvalidArgument (nichts vorgeladen, Zeit negativ oder nicht endlich).
    Result<void> go(double fadeSeconds);

    /// Schiebt einen laufenden Fade um `seconds` weiter (negativ oder nicht endlich zählt als 0).
    /// Liefert true, wenn sich dadurch ein Wert im Graphen geändert hat.
    bool advance(double seconds);

    /// Hat arm() oder start() den Aufbau des Graphen verändert (Nodes kamen oder gingen)? Wird mit dem
    /// Abfragen zurückgesetzt. Danach genügt ein Wertabgleich, bis der nächste Look geladen wird.
    [[nodiscard]] bool takeStructureChanged() noexcept;

    [[nodiscard]] const graph::Graph& graph() const noexcept { return graph_; }
    [[nodiscard]] const graph::PortRef& target() const noexcept { return target_; }
    [[nodiscard]] LiveStatus status() const noexcept;

private:
    enum class Deck { A, B };
    struct Slot {
        std::vector<NodeId> nodes;
        int look{-1};
    };

    [[nodiscard]] Slot& slot(Deck deck) noexcept { return deck == Deck::A ? a_ : b_; }
    Result<void> load(Deck deck, const project::Look& look, int index);
    void clear(Deck deck);
    void fill(Deck deck);  // durchsichtiger Platzhalter in einem leeren Deck
    void finishFade();
    void setAmount(double amount);
    [[nodiscard]] static Deck other(Deck deck) noexcept { return deck == Deck::A ? Deck::B : Deck::A; }

    graph::Graph graph_;
    graph::PortRef target_;
    NodeId mixer_;
    Slot a_;
    Slot b_;
    Deck visible_{Deck::A};
    bool armed_{false};
    bool fading_{false};
    double progress_{1.0};
    double fadeSeconds_{0.0};
    double amount_{0.0};
    bool structureChanged_{false};
};

}  // namespace whitehole::runtime
