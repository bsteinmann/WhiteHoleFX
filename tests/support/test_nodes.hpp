// test_nodes.hpp: kleines Node-Verzeichnis für Graph-Tests (unabhängig von den Built-ins).
#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "whitehole/core/types.hpp"
#include "whitehole/graph/node_registry_impl.hpp"

namespace whitehole::test {

inline graph::PortDescriptor inPort(const std::string& key, graph::PortType type,
                                    bool required = false) {
    return graph::PortDescriptor{key, key, type, graph::PortDirection::Input, required};
}

inline graph::PortDescriptor outPort(const std::string& key, graph::PortType type) {
    return graph::PortDescriptor{key, key, type, graph::PortDirection::Output, false};
}

inline void mustRegister(graph::NodeRegistry& registry, graph::NodeDescriptor descriptor) {
    if (!registry.registerNode(std::move(descriptor)).ok()) {
        std::abort();  // Fehler im Testaufbau, kein Testergebnis
    }
}

/// Typen: test.source (Field-Ausgang, vier Parameter), test.filter (ein Pflicht-
/// Eingang), test.mixer (a, b), test.label (Text-Ausgang), test.scalar (Scalar-Ausgang).
inline std::shared_ptr<graph::NodeRegistry> makeTestRegistry() {
    using graph::NodeDescriptor;
    using graph::ParameterDescriptor;
    using graph::PortType;

    auto registry = std::make_shared<graph::NodeRegistry>();

    NodeDescriptor source;
    source.typeId = "test.source";
    source.displayName = "Source";
    source.category = "Test";
    source.ports = {outPort("out", PortType::Field)};
    source.parameters = {
        ParameterDescriptor{"gain", "Gain", 0.5, 0.0, 1.0},
        ParameterDescriptor{"count", "Count", std::int64_t{1}, 0.0, 10.0},
        ParameterDescriptor{"tint", "Tint", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}, std::nullopt,
                            std::nullopt},
        ParameterDescriptor{"name", "Name", std::string{"x"}, std::nullopt, std::nullopt},
    };
    mustRegister(*registry, source);

    NodeDescriptor filter;
    filter.typeId = "test.filter";
    filter.displayName = "Filter";
    filter.category = "Test";
    filter.ports = {inPort("in", PortType::Field, true), outPort("out", PortType::Field)};
    mustRegister(*registry, filter);

    NodeDescriptor mixer;
    mixer.typeId = "test.mixer";
    mixer.displayName = "Mixer";
    mixer.category = "Test";
    mixer.ports = {inPort("a", PortType::Field, true), inPort("b", PortType::Field, true),
                   outPort("out", PortType::Field)};
    mustRegister(*registry, mixer);

    NodeDescriptor label;
    label.typeId = "test.label";
    label.displayName = "Label";
    label.category = "Test";
    label.ports = {outPort("out", PortType::Text)};
    mustRegister(*registry, label);

    NodeDescriptor scalar;
    scalar.typeId = "test.scalar";
    scalar.displayName = "Scalar";
    scalar.category = "Test";
    scalar.ports = {outPort("out", PortType::Scalar)};
    mustRegister(*registry, scalar);

    return registry;
}

}  // namespace whitehole::test
