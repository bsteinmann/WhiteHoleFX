// dmx_listener.hpp: wer einen empfangenen DMX-Kanal liest - ein Node, oder Master-Helligkeit/Live-DMX.
//
// Warum es das gibt: Schritt 4 des DMX-Patch/Monitor-Gitters (siehe CLAUDE.md), das "Wer hört zu"-Overlay
// im DMX-Monitor (Eingang). findDmxUsage (dmx_usage.hpp) sagt nur, OB ein Universe gebraucht wird; hier
// geht es genauer: welcher einzelne Kanal wird von wem gelesen. Reine Lesefunktion, ändert nichts. Player
// prüft danach zusätzlich Master-Helligkeit und Live-DMX (siehe Player::dmxListenerAt) - die stehen in den
// Auftritts-Einstellungen, nicht im Graphen, darum nicht hier.
//
// findDmxValueNodes (nachträglich freigegebene Erweiterung): listet alle wh.value.dmx/wh.value.dmxcolor-
// Nodes EINES Graphen auf, unabhängig davon, wohin sie gerade zeigen - für die Rechtsklick-Kategorie "Node"
// im DMX-Monitor (Eingang; hieß bis zur Vereinheitlichung unten "DMX Out" - umbenannt, weil das mit dem
// gleichnamigen Ausgabe-Node "DMX Out" (wh.output.dmx) verwechselt wurde), die die Adresse eines bestehenden
// Nodes direkt auf den angeklickten Kanal patcht, statt das im Node-Editor von Hand einzustellen.
// Player::dmxValueNodes() (player_edit.cpp) ruft das sowohl für den gerade im Editor offenen Graphen
// (project_->graph) als auch für jeden gespeicherten Look (project_->looks[i].graph) auf - ein Look ist ein
// eigener kleiner Graph (siehe project.hpp), der nur während der Bearbeitung in den Editor geladen ist; ein
// Node darin bleibt sonst für findDmxValueNodes(nur project_->graph) unsichtbar, obwohl er zum Projekt
// gehört. DmxValueNodeRef::lookIndex sagt, aus welchem Look (falls überhaupt) ein Treffer stammt, damit
// Studio weiß, in welchem Graphen es patchen muss.
//
// findDmxOutputValueNodes (nachträglich freigegebene Erweiterung, vom Nutzer am 15.11. gewünscht): dasselbe
// Prinzip, aber für die Schreib-Richtung - wh.output.dmx/wh.output.dmxcolor-Nodes, für die Rechtsklick-
// Kategorie "Node" im DMX-Ausgangs-Monitor. Anders als bei den Lese-Nodes liegt die Adresse eines solchen
// Nodes nicht in seinen eigenen Parametern, sondern in project_->dmxPatches (siehe output_plan.hpp,
// DmxChannelPatch): der Node schreibt nur in einen internen Pool-Kanal (sein Parameter "channel"), die
// Projektdatei-Anweisung "dmxpatch" bildet den Pool-Bereich auf einen echten Controller/Universe/Kanal ab.
// DmxOutputValueNodeRef trägt darum zusätzlich poolChannel/count (aus den aktuellen Node-Parametern
// berechnet, siehe dmxCount() in der .cpp) statt einer eigenen Adresse - Player::patchDmxOutputValueNode()
// (player_layout.cpp) legt mit diesen beiden Werten den passenden project_->dmxPatches-Eintrag an oder
// aktualisiert ihn. Nutzer-Entscheidung (AskUserQuestion, 15.11.): die Richtung passend zum Monitor - nur
// Lese-Nodes im Eingangs-, nur Schreib-Nodes im Ausgangs-Monitor, nicht alle vier Typen in beiden (die
// beiden Adressräume - empfangenes und gesendetes DMX - sind unabhängig, auch bei gleicher Universe-Nummer).
//
// DmxInputListener::Kind (nachträglich freigegebene Erweiterung, zweiter Teil des 15.11.-Vorschlags "farbig
// hinterlegen"): grob, WELCHER Art der Hörer ist - Studio-Schicht (dmx_cell_colors.hpp) färbt Zellen danach
// ein. Node für einen Graph-Node (inkl. per "Node" gepatchter Nodes), Master/LiveSelect/LiveFade für die
// drei Auftritts-Einstellungen ohne eigenen Node.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"  // NodeId
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/input/dmx_store.hpp"  // input::DmxProtocol

namespace whitehole::runtime {

struct DmxInputListener {
    enum class Kind { Node, Master, LiveSelect, LiveFade };
    Kind kind{Kind::Node};
    std::string name;          // Node-Beschriftung, oder "Master-Helligkeit" / "Live-Look-Auswahl" / "Live-Überblendzeit"
    std::uint32_t channel{1};  // erster gelesener Kanal (1-basiert)
    std::uint32_t count{1};    // wie viele Kanäle ab channel (16 Bit: 2, DMX Color: 3 oder 4)
};

/// Sucht unter den Nodes wh.value.dmx/wh.value.dmxcolor des Graphen einen, der den angegebenen Kanal liest.
/// Nullopt: kein Node im Graphen liest ihn.
[[nodiscard]] std::optional<DmxInputListener> findDmxListenerInGraph(const graph::Graph& graph, input::DmxProtocol protocol,
                                                                      std::uint32_t universe, std::uint32_t channel);

struct DmxValueNodeRef {
    NodeId id;
    std::string label;  // Beschriftung des Nodes (umbenennbar, siehe NodeInstance::label)
    /// nullopt: Node steht im gerade im Editor offenen Graphen (project_->graph). Sonst: Index in
    /// project_->looks - der Node steht in diesem Look, der momentan NICHT im Editor geladen ist.
    std::optional<std::size_t> lookIndex;
};

/// Alle wh.value.dmx/wh.value.dmxcolor-Nodes des Graphen, in Graph-Reihenfolge - für die Rechtsklick-
/// Kategorie "Node" im DMX-Monitor (siehe oben). Leer, wenn der Graph keinen solchen Node enthält.
[[nodiscard]] std::vector<DmxValueNodeRef> findDmxValueNodes(const graph::Graph& graph);

struct DmxOutputValueNodeRef {
    NodeId id;
    std::string label;  // Beschriftung des Nodes (umbenennbar, siehe NodeInstance::label)
    /// Wie DmxValueNodeRef::lookIndex: nullopt = Node steht im gerade offenen Graphen, sonst Index in
    /// project_->looks.
    std::optional<std::size_t> lookIndex;
    std::uint16_t poolChannel{1};  // aktueller Wert des Node-Parameters "channel" (interner Pool, 1..512)
    std::uint16_t count{1};        // wie viele Pool-Kanäle ab poolChannel (abhängig vom Typ/Parametern)
};

/// Alle wh.output.dmx/wh.output.dmxcolor-Nodes des Graphen, in Graph-Reihenfolge - für die Rechtsklick-
/// Kategorie "Node" im DMX-Ausgangs-Monitor (siehe oben). Leer, wenn der Graph keinen solchen Node enthält.
[[nodiscard]] std::vector<DmxOutputValueNodeRef> findDmxOutputValueNodes(const graph::Graph& graph);

}  // namespace whitehole::runtime
