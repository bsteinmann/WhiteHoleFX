// node_placement.hpp: wohin ein eingerasteter Node im Editor gesetzt wird und welche Nodes dafür Platz machen.
//
// Warum es das gibt: Ein Node, der in ein Kabel oder an einen Node eingerastet wird (node_docking.hpp), soll nicht
// unter der Maus landen und andere Nodes überdecken. Er sitzt stattdessen zwischen Quelle und Ziel (oder links
// bzw. rechts neben dem Zielnode); fehlt der Platz, rücken das Ziel und alles, was dahinter hängt, nach rechts. Die
// Rechnung steht hier als reine Funktion (ohne Fenster), damit sie getestet wird (tests/graph/node_placement_test.cpp).
// Die Größen der Nodes kennt nur der Editor; er übergibt sie als Rechtecke.
#pragma once

#include <utility>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/node_docking.hpp"

namespace whitehole::graph {

/// Ein Node im Editor: linke obere Ecke und Größe (Szenenkoordinaten).
struct NodeRect {
    NodeId id;
    float x{0.0F};
    float y{0.0F};
    float w{0.0F};
    float h{0.0F};
};

using NodeMove = std::pair<NodeId, Vec2>;  // Node und seine neue linke obere Ecke

struct Placement {
    Vec2 position;                 // linke obere Ecke des neuen Nodes
    std::vector<NodeMove> moved;   // Nodes, die dafür Platz machen
};

/// Platziert einen Node der Größe `size`, der nach `plan` einrastet. `mouse` ist die Ecke, wenn nichts Besseres
/// zu berechnen ist (kein Platz zum Berechnen, unbekannte Nodes). Der Graph liefert, was hinter einem Node hängt.
[[nodiscard]] Placement placeDockedNode(const Graph& graph, const std::vector<NodeRect>& rects, const DockPlan& plan,
                                        const DockTarget& target, Vec2 size, Vec2 mouse);

}  // namespace whitehole::graph
