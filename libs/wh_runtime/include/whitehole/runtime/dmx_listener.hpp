// dmx_listener.hpp: wer einen empfangenen DMX-Kanal liest - ein Node, oder Master-Helligkeit/Live-DMX.
//
// Warum es das gibt: Schritt 4 des DMX-Patch/Monitor-Gitters (siehe CLAUDE.md), das "Wer hört zu"-Overlay
// im DMX-Monitor (Eingang). findDmxUsage (dmx_usage.hpp) sagt nur, OB ein Universe gebraucht wird; hier
// geht es genauer: welcher einzelne Kanal wird von wem gelesen. Reine Lesefunktion, ändert nichts. Player
// prüft danach zusätzlich Master-Helligkeit und Live-DMX (siehe Player::dmxListenerAt) - die stehen in den
// Auftritts-Einstellungen, nicht im Graphen, darum nicht hier.
//
// findDmxValueNodes (nachträglich freigegebene Erweiterung): listet alle wh.value.dmx/wh.value.dmxcolor-
// Nodes EINES Graphen auf, unabhängig davon, wohin sie gerade zeigen - für die Rechtsklick-Kategorie
// "DMX Out" im DMX-Monitor, die die Adresse eines bestehenden Nodes direkt auf den angeklickten Kanal
// patcht, statt das im Node-Editor von Hand einzustellen. Player::dmxValueNodes() (player_edit.cpp) ruft
// das sowohl für den gerade im Editor offenen Graphen (project_->graph) als auch für jeden gespeicherten
// Look (project_->looks[i].graph) auf - ein Look ist ein eigener kleiner Graph (siehe project.hpp), der nur
// während der Bearbeitung in den Editor geladen ist; ein Node darin bleibt sonst für findDmxValueNodes(nur
// project_->graph) unsichtbar, obwohl er zum Projekt gehört. DmxValueNodeRef::lookIndex sagt, aus welchem
// Look (falls überhaupt) ein Treffer stammt, damit Studio weiß, in welchem Graphen es patchen muss.
//
// DmxInputListener::Kind (nachträglich freigegebene Erweiterung, zweiter Teil des 15.11.-Vorschlags "farbig
// hinterlegen"): grob, WELCHER Art der Hörer ist - Studio-Schicht (dmx_cell_colors.hpp) färbt Zellen danach
// ein. Node für einen Graph-Node (inkl. per "DMX Out" gepatchter Nodes), Master/LiveSelect/LiveFade für die
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
/// Kategorie "DMX Out" im DMX-Monitor (siehe oben). Leer, wenn der Graph keinen solchen Node enthält.
[[nodiscard]] std::vector<DmxValueNodeRef> findDmxValueNodes(const graph::Graph& graph);

}  // namespace whitehole::runtime
