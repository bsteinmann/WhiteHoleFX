// dmx_usage.hpp: welche DMX-Universen die DMX-Nodes eines Graphen lesen.
//
// Warum es das gibt: Der Empfang soll nur das hören, was das Projekt braucht. Für sACN heißt das
// konkret: nur den Multicast-Gruppen dieser Universen beitreten. Außerdem kann die Oberfläche
// anzeigen "dieses Projekt erwartet DMX, der Empfang ist aber aus".
#pragma once

#include <cstdint>
#include <set>

#include "whitehole/graph/graph_model.hpp"

namespace whitehole::runtime {

struct DmxUsage {
    std::set<std::uint32_t> artnet;  // Art-Net-Universen (Port-Address)
    std::set<std::uint32_t> sacn;    // sACN-Universen

    [[nodiscard]] bool any() const { return !artnet.empty() || !sacn.empty(); }
};

/// Durchsucht alle Nodes vom Typ wh.value.dmx und wh.value.dmxcolor nach Quelle und Universe.
[[nodiscard]] DmxUsage findDmxUsage(const graph::Graph& graph);

}  // namespace whitehole::runtime
