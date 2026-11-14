// graph_layout.hpp: automatische Anordnung der Nodes für den Node-Editor.
//
// Warum es diese Datei gibt: Dateien von Hand haben keine Positionen (`at` fehlt, alle Nodes
// liegen auf 0 0). Der Editor soll sie trotzdem lesbar zeigen. Die Anordnung ist reine Rechnung
// auf dem Graphen und braucht kein Qt, also wird sie hier getestet.
//
// Regel: Spalten von links nach rechts. Ein Node steht eine Spalte rechts von seinem am
// weitesten rechts stehenden Zulieferer; Quellen stehen in Spalte 0. Innerhalb einer Spalte gilt
// die Reihenfolge im Graphen, mit so viel Abstand untereinander, wie der jeweils vorherige Node
// (geschätzt aus seiner Portzahl) tatsächlich hoch ist - sonst überlappen sich Nodes mit vielen
// Parametern (jeder bekommt automatisch einen Eingangsport, siehe addModulationPorts) mit ihrem
// Nachbarn in derselben Spalte.
#pragma once

#include <vector>

#include "whitehole/core/types.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::runtime {

struct LayoutSpacing {
    float column{260.0F};
    float row{24.0F};  // Lücke UNTER einem Node, zusätzlich zu seiner geschätzten Höhe (nicht mehr die volle Zeile)
};

/// Position je Node, in der Reihenfolge von graph.nodes().
[[nodiscard]] std::vector<Vec2> layerLayout(const graph::Graph& graph, const LayoutSpacing& spacing = {});

/// Liegen mindestens zwei Nodes, aber alle auf (0, 0)? Dann fehlen Positionen.
[[nodiscard]] bool needsLayout(const graph::Graph& graph);

/// Wendet layerLayout an, wenn needsLayout() gilt. Liefert, ob etwas geändert wurde.
bool applyLayoutIfMissing(graph::Graph& graph, const LayoutSpacing& spacing = {});

}  // namespace whitehole::runtime
