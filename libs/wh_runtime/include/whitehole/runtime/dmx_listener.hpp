// dmx_listener.hpp: wer einen empfangenen DMX-Kanal liest - ein Node, oder Master-Helligkeit/Live-DMX.
//
// Warum es das gibt: Schritt 4 des DMX-Patch/Monitor-Gitters (siehe CLAUDE.md), das "Wer hört zu"-Overlay
// im DMX-Monitor (Eingang). findDmxUsage (dmx_usage.hpp) sagt nur, OB ein Universe gebraucht wird; hier
// geht es genauer: welcher einzelne Kanal wird von wem gelesen. Reine Lesefunktion, ändert nichts. Player
// prüft danach zusätzlich Master-Helligkeit und Live-DMX (siehe Player::dmxListenerAt) - die stehen in den
// Auftritts-Einstellungen, nicht im Graphen, darum nicht hier.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/input/dmx_store.hpp"  // input::DmxProtocol

namespace whitehole::runtime {

struct DmxInputListener {
    std::string name;          // Node-Beschriftung, oder "Master-Helligkeit" / "Live-Look-Auswahl" / "Live-Überblendzeit"
    std::uint32_t channel{1};  // erster gelesener Kanal (1-basiert)
    std::uint32_t count{1};    // wie viele Kanäle ab channel (16 Bit: 2, DMX Color: 3 oder 4)
};

/// Sucht unter den Nodes wh.value.dmx/wh.value.dmxcolor des Graphen einen, der den angegebenen Kanal liest.
/// Nullopt: kein Node im Graphen liest ihn.
[[nodiscard]] std::optional<DmxInputListener> findDmxListenerInGraph(const graph::Graph& graph, input::DmxProtocol protocol,
                                                                      std::uint32_t universe, std::uint32_t channel);

}  // namespace whitehole::runtime
