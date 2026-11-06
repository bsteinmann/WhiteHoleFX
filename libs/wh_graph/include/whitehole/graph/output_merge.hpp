// output_merge.hpp: Der unsichtbare Adder für mehrere Output-Nodes.
// Warum es das gibt: Jeder Output-Node ist ein eigener, gleichberechtigter Ausgang. Damit das Entfernen oder Leeren
// einer Effektkette die anderen nicht lahmlegt, stehen zwischen ihnen keine gespeicherten Kabel. Stattdessen setzt
// der Compiler sie zur Laufzeit auf einer Kopie des Graphen über Add-Nodes zusammen; im Projekt sieht man davon nichts.
#pragma once

#include <optional>
#include <utility>

#include "whitehole/graph/graph_model.hpp"

namespace whitehole::graph {

/// Typ-ID des Output-Nodes.
inline constexpr const char* kOutputNodeType = "wh.output.canvas";

/// Ergebnis von mergeOutputs: der Graph mit den ergänzten Adder-Nodes und der neue Ausgang.
struct MergedOutputs {
    Graph graph;
    PortRef target;
};

/// Liegt `target` auf einem Output-Node und gibt es weitere, vom Rest unabhängige Outputs mit angeschlossener
/// `source`, so entsteht eine Kopie des Graphen, in der alle diese Outputs über `wh.blend.add` summiert werden.
/// Ein Output ohne `source` (frisch angelegt) bleibt dunkel und stört die anderen nicht; ist er der Ausgang des Projekts,
/// tritt der erste Output mit Effekt an seine Stelle. Outputs ohne `source` zählen nicht mit; Outputs, die per `behind` hinter einem anderen hängen (alte Dateien),
/// auch nicht. Sonst (oder wenn die Registry keinen Add-Node kennt) kommt nichts zurück: Der Graph bleibt wie er ist.
[[nodiscard]] std::optional<MergedOutputs> mergeOutputs(const Graph& graph, const PortRef& target);

}  // namespace whitehole::graph
