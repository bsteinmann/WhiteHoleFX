// test_field_nodes.hpp: kleine Field-Nodes mit Snippets für die Compiler-Tests.
#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_nodes.hpp"
#include "whitehole/compiler/node_definition.hpp"

namespace whitehole::test {

struct CompilerFixture {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary library;
};

inline compiler::NodeDefinition fieldNode(std::string typeId,
                                          std::vector<graph::PortDescriptor> ports,
                                          std::vector<graph::ParameterDescriptor> parameters,
                                          std::string body, std::string prelude = {}) {
    compiler::NodeDefinition definition;
    definition.descriptor.displayName = typeId;
    definition.descriptor.typeId = std::move(typeId);
    definition.descriptor.domain = graph::NodeDomain::Field;
    definition.descriptor.ports = std::move(ports);
    definition.descriptor.parameters = std::move(parameters);
    definition.snippet.body = std::move(body);
    definition.snippet.prelude = std::move(prelude);
    return definition;
}

inline graph::ParameterDescriptor parameter(const std::string& key, graph::ParameterValue value) {
    return graph::ParameterDescriptor{key, key, std::move(value), std::nullopt, std::nullopt};
}

/// Typen (alle in der Field-Domäne, sofern nicht anders genannt):
///  c.const      Ausgang; Parameter color, level, label (Text), offset, dir, flag
///  c.shift      Eingang source (Pflicht); verschiebt die Koordinate um amount
///  c.add        Eingänge a (Pflicht) und b (optional)
///  c.noise/2    Ausgang; gemeinsames Prelude (c_hash)
///  c.modulated  Field-Eingang source plus Scalar-Eingang amount
///  c.raster     Raster-Domäne;  c.twoouts: zwei Field-Ausgänge
///  c.badbody / c.badparam: Snippet mit unbekanntem Port beziehungsweise Text-Parameter
///  c.nosnippet  nur Beschreibung, kein Snippet
///  c.knob       Field-Eingang source plus modulierbare Parameter amount (Scalar, 0..1),
///               flag (Boolean) und tint (Color); Eingänge heißen wie die Parameter
///  Quellen (CPU-Domäne): c.scalar (Scalar), c.color (Color), c.flag (Boolean),
///               c.image (Image, nur um "Bild als Feld-Eingang" zu prüfen)
inline CompilerFixture makeCompilerFixture() {
    using graph::ParameterDescriptor;
    using graph::PortType;

    CompilerFixture fixture;
    std::vector<compiler::NodeDefinition> definitions;

    definitions.push_back(fieldNode(
        "c.const", {outPort("out", PortType::Field)},
        {ParameterDescriptor{"color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}, std::nullopt,
                             std::nullopt},
         ParameterDescriptor{"level", "Level", 1.0, 0.0, 1.0},
         parameter("label", std::string{"x"}), parameter("offset", Vec2{0.1F, 0.2F}),
         parameter("dir", Vec3{0.3F, 0.4F, 0.5F}), parameter("flag", true)},
        "return @param(color) * @param(level);"));

    definitions.push_back(fieldNode(
        "c.shift", {inPort("source", PortType::Field, true), outPort("out", PortType::Field)},
        {parameter("amount", 0.0)},
        "return @in(source, p + vec3<f32>(@param(amount), 0.0, 0.0));"));

    definitions.push_back(fieldNode(
        "c.add",
        {inPort("a", PortType::Field, true), inPort("b", PortType::Field), outPort("out", PortType::Field)},
        {}, "return @in(a, p) + @in(b, p);"));

    const std::string hashPrelude = "fn c_hash(x: f32) -> f32 {\n    return fract(sin(x) * 43758.5453);\n}\n";
    definitions.push_back(fieldNode("c.noise", {outPort("out", PortType::Field)}, {},
                                    "return vec4<f32>(c_hash(p.x + @time));", hashPrelude));
    definitions.push_back(fieldNode("c.noise2", {outPort("out", PortType::Field)}, {},
                                    "return vec4<f32>(c_hash(p.y));", hashPrelude));

    definitions.push_back(fieldNode("c.modulated",
                                    {inPort("source", PortType::Field, true),
                                     inPort("amount", PortType::Scalar), outPort("out", PortType::Field)},
                                    {}, "return @in(source, p);"));

    definitions.push_back(fieldNode(
        "c.knob",
        {inPort("source", PortType::Field, true), inPort("amount", PortType::Scalar),
         inPort("flag", PortType::Boolean), inPort("tint", PortType::Color),
         outPort("out", PortType::Field)},
        {ParameterDescriptor{"amount", "Amount", 0.5, 0.0, 1.0}, parameter("flag", true),
         parameter("tint", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        "return @in(source, p) * @param(tint) * @param(amount) * @param(flag);"));

    definitions.push_back(fieldNode("c.raster", {outPort("out", PortType::Field)}, {},
                                    "return vec4<f32>(0.0);"));
    definitions.back().descriptor.domain = graph::NodeDomain::Raster;

    definitions.push_back(fieldNode(
        "c.twoouts", {outPort("out", PortType::Field), outPort("out2", PortType::Field)}, {},
        "return vec4<f32>(0.0);"));

    definitions.push_back(fieldNode("c.badbody", {outPort("out", PortType::Field)}, {},
                                    "return @in(nope, p);"));
    definitions.push_back(fieldNode("c.badparam", {outPort("out", PortType::Field)},
                                    {parameter("label", std::string{"x"})},
                                    "return vec4<f32>(@param(label));"));

    if (!compiler::registerDefinitions(definitions, *fixture.registry, fixture.library).ok()) {
        std::abort();
    }

    graph::NodeDescriptor noSnippet;
    noSnippet.typeId = "c.nosnippet";
    noSnippet.ports = {outPort("out", PortType::Field)};
    mustRegister(*fixture.registry, noSnippet);

    const auto registerSource = [&fixture](const char* typeId, PortType type) {
        graph::NodeDescriptor source;
        source.typeId = typeId;
        source.domain = graph::NodeDomain::Cpu;
        source.ports = {outPort("out", type)};
        mustRegister(*fixture.registry, source);
    };
    registerSource("c.scalar", PortType::Scalar);
    registerSource("c.color", PortType::Color);
    registerSource("c.flag", PortType::Boolean);
    registerSource("c.image", PortType::Image);

    return fixture;
}

}  // namespace whitehole::test
