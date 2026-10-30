// graph_topology.hpp: reine Graphenalgorithmen über Node-IDs und Verbindungen.
#pragma once

#include <span>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::graph {

/// Führt von `from` ein Weg über Verbindungen (Ausgang -> Eingang) nach `to`?
/// Ein Node erreicht sich selbst. Dient der Zyklenprüfung: Die Verbindung
/// a -> b erzeugt genau dann einen Zyklus, wenn isReachable(b, a) gilt.
[[nodiscard]] bool isReachable(std::span<const Connection> connections, NodeId from, NodeId to);

/// Auswertungsreihenfolge (Kahn): Jeder Node steht nach allen seinen Vorgängern.
/// Unabhängige Nodes behalten ihre Reihenfolge aus `nodes`, das Ergebnis ist
/// also deterministisch.
/// Fehler: CycleDetected bei einem Zyklus, NotFound wenn eine Verbindung auf
/// einen Node verweist, der nicht in `nodes` steht.
[[nodiscard]] Result<std::vector<NodeId>> topologicalOrder(std::span<const NodeId> nodes,
                                                           std::span<const Connection> connections);

/// Bequemer Aufruf für einen ganzen Graphen.
[[nodiscard]] Result<std::vector<NodeId>> topologicalOrder(const Graph& graph);

}  // namespace whitehole::graph
