// builtin_internal.hpp: Hilfen, damit die Node-Definitionen kurz und lesbar bleiben.
// Nur für wh_nodes selbst gedacht, kein öffentlicher Header.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "whitehole/compiler/node_definition.hpp"
#include "whitehole/graph/parameter_resolve.hpp"
#include "whitehole/values/cpu_kernel.hpp"

namespace whitehole::nodes::detail {

inline graph::PortDescriptor fieldOutput() {
    return graph::PortDescriptor{"out", "Out", graph::PortType::Field, graph::PortDirection::Output,
                                 false};
}

inline graph::PortDescriptor fieldInput(std::string key, std::string displayName) {
    return graph::PortDescriptor{std::move(key), std::move(displayName), graph::PortType::Field,
                                 graph::PortDirection::Input, true};
}

/// Hilfsfunktionen für Farben mit Transparenz (Alpha). Alle Farben im Field sind "gerade"
/// RGBA (Alpha getrennt von RGB). Beim Überblenden und Addieren wird mit vormultiplizierten
/// Werten gerechnet, sonst färbt ein transparenter Rand (RGB schwarz, Alpha 0) die Mischung dunkel.
/// Bei deckenden Farben (Alpha 1) kommt dasselbe heraus wie ein einfaches mix() oder clamp(a + b).
/// Der Ausgabe-Pass multipliziert zuletzt mit Alpha: Transparent heißt "LED aus".
/// Alle Nodes, die diese Funktionen brauchen, geben dieses Prelude an; gleiche Preludes werden
/// vom Compiler zusammengelegt.
inline constexpr const char* kColorPrelude = R"wgsl(fn wh_mix_color(a: vec4<f32>, b: vec4<f32>, t: f32) -> vec4<f32> {
    let pa = vec4<f32>(a.rgb * a.a, a.a);
    let pb = vec4<f32>(b.rgb * b.a, b.a);
    let m = mix(pa, pb, t);
    return vec4<f32>(m.rgb / max(m.a, 0.000001), m.a);
}

fn wh_add_color(a: vec4<f32>, b: vec4<f32>) -> vec4<f32> {
    let alpha = clamp(a.a + b.a, 0.0, 1.0);
    let light = clamp(a.rgb * a.a + b.rgb * b.a, vec3<f32>(0.0, 0.0, 0.0), vec3<f32>(1.0, 1.0, 1.0));
    return vec4<f32>(min(light / max(alpha, 0.000001), vec3<f32>(1.0, 1.0, 1.0)), alpha);
}
)wgsl";

/// Zahl mit Standardwert und optionalem Bereich (Werte werden darauf begrenzt).
inline graph::ParameterDescriptor number(std::string key, std::string displayName, double value,
                                         std::optional<double> minValue = std::nullopt,
                                         std::optional<double> maxValue = std::nullopt) {
    return graph::ParameterDescriptor{std::move(key), std::move(displayName), value, minValue,
                                      maxValue};
}

/// Ganzzahl (zum Beispiel Auswahl einer Wellenform) mit optionalem Bereich.
inline graph::ParameterDescriptor integer(std::string key, std::string displayName, std::int64_t value,
                                          std::optional<double> minValue = std::nullopt,
                                          std::optional<double> maxValue = std::nullopt) {
    return graph::ParameterDescriptor{std::move(key), std::move(displayName), value, minValue,
                                      maxValue};
}

inline graph::ParameterDescriptor toggle(std::string key, std::string displayName, bool value) {
    return graph::ParameterDescriptor{std::move(key), std::move(displayName), value, std::nullopt,
                                      std::nullopt};
}

inline graph::ParameterDescriptor color(std::string key, std::string displayName, ColorRgba value) {
    return graph::ParameterDescriptor{std::move(key), std::move(displayName), value, std::nullopt,
                                      std::nullopt};
}

inline graph::PortDescriptor valueOutput(std::string key, std::string displayName,
                                         graph::PortType type) {
    return graph::PortDescriptor{std::move(key), std::move(displayName), type,
                                 graph::PortDirection::Output, false};
}

/// Ein Field-Node mit Beschreibung und Snippet. Jeder modulierbare Parameter
/// bekommt automatisch einen Eingang gleichen Schlüssels (siehe parameter_resolve.hpp).
inline compiler::NodeDefinition fieldNode(std::string typeId, std::string displayName,
                                          std::string category,
                                          std::vector<graph::PortDescriptor> ports,
                                          std::vector<graph::ParameterDescriptor> parameters,
                                          std::string body, std::string prelude = {}) {
    compiler::NodeDefinition definition;
    definition.descriptor.typeId = std::move(typeId);
    definition.descriptor.displayName = std::move(displayName);
    definition.descriptor.category = std::move(category);
    definition.descriptor.domain = graph::NodeDomain::Field;
    definition.descriptor.ports = std::move(ports);
    definition.descriptor.parameters = std::move(parameters);
    graph::addModulationPorts(definition.descriptor);
    definition.snippet.body = std::move(body);
    definition.snippet.prelude = std::move(prelude);
    return definition;
}

/// Ein CPU-Node mit Beschreibung und Kernel; ebenfalls mit Modulations-Eingängen.
inline values::CpuNodeDefinition cpuNode(std::string typeId, std::string displayName,
                                         std::string category,
                                         std::vector<graph::PortDescriptor> ports,
                                         std::vector<graph::ParameterDescriptor> parameters,
                                         values::KernelFactory factory) {
    values::CpuNodeDefinition definition;
    definition.descriptor.typeId = std::move(typeId);
    definition.descriptor.displayName = std::move(displayName);
    definition.descriptor.category = std::move(category);
    definition.descriptor.domain = graph::NodeDomain::Cpu;
    definition.descriptor.ports = std::move(ports);
    definition.descriptor.parameters = std::move(parameters);
    graph::addModulationPorts(definition.descriptor);
    definition.factory = std::move(factory);
    return definition;
}

/// Fabrik für einen Kernel-Typ ohne Konstruktorargumente.
template <typename Kernel>
values::KernelFactory kernelFactory() {
    return [] { return std::make_unique<Kernel>(); };
}

std::vector<compiler::NodeDefinition> generatorNodes();
std::vector<compiler::NodeDefinition> transformNodes();
std::vector<compiler::NodeDefinition> blendNodes();
std::vector<values::CpuNodeDefinition> valueNodes();

}  // namespace whitehole::nodes::detail
