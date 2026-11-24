// benchmark_scenes.hpp: die vier fest eingebauten Szenen des Benchmarks als Node-Graph.
//
// Warum es diese Datei gibt (Entwurf docs/entwurf-benchmark.md): Messungen verschiedener Nutzer sollen vergleichbar sein.
// Darum sind die Szenen im Programm festgelegt (keine Beispieldateien, die sich ändern können), und alle benutzten Nodes
// sind zeitlich deterministisch. „Schichten“ steht für typische Projekte: drei Generatoren, zwei Mischungen und ein Warp.
#pragma once

#include <memory>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/runtime/benchmark_plan.hpp"

namespace whitehole::runtime {

struct BenchmarkSceneGraph {
    graph::Graph graph;
    graph::PortRef output;  // Field-Ausgang, der gerastert bzw. abgetastet wird
};

/// Plasma: wh.generator.plasma. Partikel: wh.generator.particles (48 Teilchen, das Maximum). Labyrinth: wh.generator.maze. Schichten:
/// Plasma + Partikel → Kaleidoskop, + Labyrinth. Fehler: die des Graphen (unbekannter Typ, wenn ein Node fehlt).
[[nodiscard]] Result<BenchmarkSceneGraph> makeBenchmarkScene(BenchmarkScene scene,
                                                             std::shared_ptr<const graph::INodeRegistry> registry);

}  // namespace whitehole::runtime
