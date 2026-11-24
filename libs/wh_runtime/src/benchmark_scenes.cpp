// benchmark_scenes.cpp: Aufbau der Benchmark-Szenen (siehe benchmark_scenes.hpp).
#include "whitehole/runtime/benchmark_scenes.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace whitehole::runtime {

namespace {

/// Kleiner Helfer, der den ersten Fehler festhält, damit der Aufbau ohne Prüfung nach jedem Schritt lesbar bleibt.
struct Builder {
    graph::Graph& graph;
    std::optional<Error> error;

    NodeId add(std::string_view type) {
        if (error) return NodeId{};
        auto r = graph.addNode(type, Vec2{});
        if (!r.ok()) {
            error = r.error();
            return NodeId{};
        }
        return r.value();
    }
    void link(NodeId from, const char* fromPort, NodeId to, const char* toPort) {
        if (error) return;
        if (auto r = graph.connect({from, fromPort}, {to, toPort}); !r.ok()) error = r.error();
    }
    void set(NodeId node, const char* key, graph::ParameterValue value) {
        if (error) return;
        if (auto r = graph.setParameter(node, key, std::move(value)); !r.ok()) error = r.error();
    }
};

}  // namespace

Result<BenchmarkSceneGraph> makeBenchmarkScene(BenchmarkScene scene, std::shared_ptr<const graph::INodeRegistry> registry) {
    BenchmarkSceneGraph result{graph::Graph{std::move(registry)}, {}};
    Builder b{result.graph, std::nullopt};
    NodeId out;
    switch (scene) {
        case BenchmarkScene::Plasma:
            out = b.add("wh.generator.plasma");
            break;
        case BenchmarkScene::Particles:
            out = b.add("wh.generator.particles");
            b.set(out, "count", graph::ParameterValue{std::int64_t{48}});
            break;
        case BenchmarkScene::Maze:
            out = b.add("wh.generator.maze");
            break;
        case BenchmarkScene::Layers: {
            const NodeId plasma = b.add("wh.generator.plasma");
            const NodeId particles = b.add("wh.generator.particles");
            const NodeId maze = b.add("wh.generator.maze");
            const NodeId mix = b.add("wh.blend.add");
            const NodeId kaleido = b.add("wh.transform.kaleidoscope");
            out = b.add("wh.blend.add");
            b.link(plasma, "out", mix, "a");
            b.link(particles, "out", mix, "b");
            b.link(mix, "out", kaleido, "source");
            b.link(kaleido, "out", out, "a");
            b.link(maze, "out", out, "b");
            break;
        }
    }
    if (b.error) {
        return *b.error;
    }
    result.output = graph::PortRef{out, "out"};
    return result;
}

}  // namespace whitehole::runtime
