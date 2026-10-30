// builtin_lab.hpp: Prüfstand für die Werte-Nodes: alle Built-ins, ein Graph, ein Evaluator.
// Gemeinsam genutzt von builtin_values_test.cpp (einzelne Nodes) und
// value_integration_test.cpp (Zusammenspiel mit dem Feldcompiler).
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"
#include "whitehole/values/value_evaluator.hpp"

namespace whitehole::test {

/// Werte-Nodes werden nur ausgewertet, wenn jemand sie braucht; `feed` hängt dafür
/// einen Field-Node (Rotate) als Verbraucher an.
struct Lab {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    graph::Graph graph{registry};
    values::ValueEvaluator evaluator{kernels};
    values::ValueStore store;
    std::uint64_t frame{0};

    Lab() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }

    NodeId add(std::string_view typeId) { return graph.addNode(typeId, Vec2{}).value(); }

    template <typename T>
    void set(NodeId node, const char* key, T value) {
        REQUIRE(graph.setParameter(node, key, graph::ParameterValue{value}).ok());
    }

    void connect(NodeId from, const char* fromPort, NodeId to, const char* toPort) {
        REQUIRE(graph.connect({from, fromPort}, {to, toPort}).ok());
    }

    /// Hängt den Ausgang an einen Drehwinkel, damit der Node einen Verbraucher hat.
    NodeId feed(NodeId from, const char* fromPort) {
        const NodeId rotate = add("wh.transform.rotate");
        connect(from, fromPort, rotate, "angle");
        return rotate;
    }

    void run(double seconds, double delta = 0.0) {
        FrameTime time;
        time.frame = frame++;
        time.time = Seconds{seconds};
        time.delta = Seconds{delta};
        REQUIRE(evaluator.evaluate(graph, time, store).ok());
    }

    double number(NodeId node, const char* port) const {
        const auto value = store.value(node, port);
        REQUIRE(value.has_value());
        return std::get<double>(*value);
    }

    /// Ein einzelner Node mit Verbraucher: Ergebnis seines Ausgangs `out` zur Zeit t.
    double sample(NodeId node, double seconds, const char* port = "out") {
        run(seconds);
        return number(node, port);
    }
};

inline float floatAt(const std::vector<std::byte>& bytes, std::uint32_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

inline const compiler::ParameterSlot* slotFor(const compiler::ParameterLayout& layout, NodeId node,
                                       std::string_view key) {
    for (const compiler::ParameterSlot& slot : layout.slots) {
        if (slot.node == node && slot.key == key) {
            return &slot;
        }
    }
    return nullptr;
}

}  // namespace whitehole::test
