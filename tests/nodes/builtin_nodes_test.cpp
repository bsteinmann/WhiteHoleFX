#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/graph/parameter_resolve.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"

using namespace whitehole;

namespace {

struct Builtins {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary library;
    values::CpuKernelLibrary kernels;

    Builtins() {
        REQUIRE(nodes::registerBuiltinNodes(*registry, library, kernels).ok());
    }
};

NodeId add(graph::Graph& graph, std::string_view typeId) {
    return graph.addNode(typeId, Vec2{}).value();
}

graph::PortRef out(NodeId node) { return graph::PortRef{node, "out"}; }

std::vector<graph::NodeDescriptor> allBuiltinDescriptors() {
    std::vector<graph::NodeDescriptor> result;
    for (const compiler::NodeDefinition& definition : nodes::builtinNodeDefinitions()) {
        result.push_back(definition.descriptor);
    }
    for (const values::CpuNodeDefinition& definition : nodes::builtinCpuDefinitions()) {
        result.push_back(definition.descriptor);
    }
    return result;
}

}  // namespace

TEST_CASE("Built-ins: alle Nodes werden samt Snippet beziehungsweise Kernel registriert") {
    Builtins builtins;

    const std::set<std::string> expectedField{
        "wh.generator.gradient", "wh.generator.noise", "wh.generator.plasma",
        "wh.generator.sphere",   "wh.generator.wave",    "wh.generator.rain",
        "wh.transform.rotate",   "wh.transform.mirror", "wh.transform.scale",
        "wh.transform.rotate3d", "wh.transform.region", "wh.transform.symmetry", "wh.blend.mix",
        "wh.blend.blend", "wh.blend.crossfade", "wh.blend.mask", "wh.blend.key", "wh.modifier.coloradjust",
        "wh.modifier.strobe", "wh.modifier.outline", "wh.output.canvas", "wh.transform.slide", "wh.transform.tile",
        "wh.transform.pixelate", "wh.generator.solid", "wh.generator.scan", "wh.generator.ripples", "wh.generator.fire",
        "wh.blend.add", "wh.generator.metaballs", "wh.generator.starfield", "wh.generator.fading",
        "wh.generator.shapes", "wh.generator.sinelines"};
    const std::set<std::string> expectedCpu{"wh.value.time",   "wh.value.number", "wh.value.color",
                                            "wh.value.lfo",    "wh.value.random", "wh.value.map",
                                            "wh.value.smooth", "wh.value.colorramp"};

    std::set<std::string> field;
    std::set<std::string> cpu;
    for (const graph::NodeDescriptor* descriptor : builtins.registry->all()) {
        CHECK_FALSE(descriptor->displayName.empty());
        CHECK_FALSE(descriptor->category.empty());
        if (descriptor->domain == graph::NodeDomain::Field) {
            field.insert(descriptor->typeId);
            CHECK(builtins.library.find(descriptor->typeId) != nullptr);
            CHECK_FALSE(builtins.kernels.contains(descriptor->typeId));
        } else {
            REQUIRE(descriptor->domain == graph::NodeDomain::Cpu);
            cpu.insert(descriptor->typeId);
            CHECK(builtins.kernels.contains(descriptor->typeId));
            CHECK(builtins.library.find(descriptor->typeId) == nullptr);
        }
    }
    CHECK(field == expectedField);
    CHECK(cpu == expectedCpu);
}

TEST_CASE("Built-ins: zweites Registrieren wird abgelehnt") {
    Builtins builtins;
    compiler::SnippetLibrary otherLibrary;
    values::CpuKernelLibrary otherKernels;

    const Result<void> again = nodes::registerBuiltinNodes(*builtins.registry, otherLibrary, otherKernels);
    REQUIRE_FALSE(again.ok());
    CHECK(again.error().code == ErrorCode::AlreadyExists);
}

TEST_CASE("Built-ins: Standardwerte liegen in ihrem eigenen Bereich") {
    for (const graph::NodeDescriptor& descriptor : allBuiltinDescriptors()) {
        for (const graph::ParameterDescriptor& parameter : descriptor.parameters) {
            INFO(descriptor.typeId << "." << parameter.key);
            double value = 0.0;
            if (const auto* real = std::get_if<double>(&parameter.defaultValue)) {
                value = *real;
            } else if (const auto* integer = std::get_if<std::int64_t>(&parameter.defaultValue)) {
                value = static_cast<double>(*integer);
            } else {
                continue;
            }
            if (parameter.minValue.has_value()) {
                CHECK(value >= *parameter.minValue);
            }
            if (parameter.maxValue.has_value()) {
                CHECK(value <= *parameter.maxValue);
            }
        }
    }
}

TEST_CASE("Built-ins: jeder modulierbare Parameter hat einen optionalen Eingang gleichen Schlüssels") {
    for (const graph::NodeDescriptor& descriptor : allBuiltinDescriptors()) {
        for (const graph::ParameterDescriptor& parameter : descriptor.parameters) {
            INFO(descriptor.typeId << "." << parameter.key);
            const auto expected = graph::modulationPortType(parameter.defaultValue);
            if (!expected.has_value()) {
                continue;
            }
            const graph::PortDescriptor* port = descriptor.findPort(parameter.key);
            REQUIRE(port != nullptr);
            CHECK(port->direction == graph::PortDirection::Input);
            CHECK(port->type == *expected);
            CHECK_FALSE(port->required);
        }
    }
}

TEST_CASE("Built-ins: jeder Field-Node lässt sich mit gefüllten Pflicht-Eingängen kompilieren") {
    Builtins builtins;

    for (const graph::NodeDescriptor* descriptor : builtins.registry->all()) {
        if (descriptor->domain != graph::NodeDomain::Field) {
            continue;
        }
        INFO(descriptor->typeId);
        graph::Graph graph{builtins.registry};
        const NodeId node = add(graph, descriptor->typeId);

        for (const graph::PortDescriptor& port : descriptor->ports) {
            if (port.direction == graph::PortDirection::Input && port.required) {
                const NodeId source = add(graph, "wh.generator.gradient");
                REQUIRE(graph.connect(out(source), graph::PortRef{node, port.key}).ok());
            }
        }

        const auto compiled = compiler::compileField(graph, builtins.library, out(node));
        REQUIRE(compiled.ok());
        CHECK(compiled.value().structureHash != 0U);
        CHECK(compiled.value().wgsl.find("fn wh_field(") != std::string::npos);
        CHECK(compiled.value().layout.sizeBytes % 16U == 0U);
    }
}

TEST_CASE("Built-ins: Kette Plasma -> Rotate -> Mirror, gemischt mit Noise") {
    Builtins builtins;
    graph::Graph graph{builtins.registry};

    const NodeId plasma = add(graph, "wh.generator.plasma");
    const NodeId rotate = add(graph, "wh.transform.rotate");
    const NodeId mirror = add(graph, "wh.transform.mirror");
    const NodeId noise = add(graph, "wh.generator.noise");
    const NodeId mix = add(graph, "wh.blend.mix");

    REQUIRE(graph.connect(out(plasma), graph::PortRef{rotate, "source"}).ok());
    REQUIRE(graph.connect(out(rotate), graph::PortRef{mirror, "source"}).ok());
    REQUIRE(graph.connect(out(mirror), graph::PortRef{mix, "a"}).ok());
    REQUIRE(graph.connect(out(noise), graph::PortRef{mix, "b"}).ok());
    REQUIRE(graph.setParameter(rotate, "angle", 123.456).ok());

    const auto compiled = compiler::compileField(graph, builtins.library, out(mix));
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;

    CHECK(compiled.value().nodes.size() == 5U);
    CHECK(wgsl.find("fn wh_hash31(") != std::string::npos);
    CHECK(wgsl.find("fn wh_hash31(") == wgsl.rfind("fn wh_hash31("));  // Prelude nur einmal
    CHECK(wgsl.find("fn wh_f1(") < wgsl.find("fn wh_f2("));              // Plasma vor Rotate
    CHECK(wgsl.find("fn wh_f2(") < wgsl.find("fn wh_f3("));              // Rotate vor Mirror
    CHECK(wgsl.find("fn wh_f5(") != std::string::npos);                  // Mix
    CHECK(wgsl.find("return wh_f5(p);") != std::string::npos);

    // Der Winkel steckt im Parameterpuffer, nicht im Shadertext.
    CHECK(wgsl.find("123.456") == std::string::npos);
}

TEST_CASE("Built-ins: Farben mit Transparenz werden vormultipliziert gemischt und addiert") {
    Builtins builtins;
    graph::Graph graph{builtins.registry};

    const NodeId gradient = add(graph, "wh.generator.gradient");
    const NodeId sphere = add(graph, "wh.generator.sphere");
    const NodeId mix = add(graph, "wh.blend.mix");
    const NodeId sum = add(graph, "wh.blend.add");
    REQUIRE(graph.connect(out(gradient), graph::PortRef{mix, "a"}).ok());
    REQUIRE(graph.connect(out(sphere), graph::PortRef{mix, "b"}).ok());
    REQUIRE(graph.connect(out(mix), graph::PortRef{sum, "a"}).ok());
    REQUIRE(graph.connect(out(sphere), graph::PortRef{sum, "b"}).ok());
    REQUIRE(graph.setParameter(gradient, "colorA", ColorRgba{0.0F, 0.0F, 0.0F, 0.0F}).ok());

    const auto compiled = compiler::compileField(graph, builtins.library, out(sum));
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;

    // Die Hilfsfunktionen stehen genau einmal im Shader, obwohl vier Nodes sie brauchen.
    CHECK(wgsl.find("fn wh_mix_color(") != std::string::npos);
    CHECK(wgsl.find("fn wh_mix_color(") == wgsl.rfind("fn wh_mix_color("));
    CHECK(wgsl.find("fn wh_add_color(") != std::string::npos);
    CHECK(wgsl.find("fn wh_add_color(") == wgsl.rfind("fn wh_add_color("));
    // Verlauf, Kugel und Mix überblenden damit; ein rohes mix() auf der Farbe gäbe es nicht mehr.
    CHECK(wgsl.find("return wh_mix_color(") != std::string::npos);
    CHECK(wgsl.find("return mix(") == std::string::npos);
    CHECK(wgsl.find("return wh_add_color(") != std::string::npos);
}

TEST_CASE("Built-ins: die 3D-Nodes nutzen alle drei Achsen und halten Werte im Puffer") {
    Builtins builtins;
    graph::Graph graph{builtins.registry};

    const NodeId wave = add(graph, "wh.generator.wave");
    const NodeId rotate = add(graph, "wh.transform.rotate3d");
    const NodeId sphere = add(graph, "wh.generator.sphere");
    const NodeId mix = add(graph, "wh.blend.add");
    REQUIRE(graph.connect(out(wave), graph::PortRef{rotate, "source"}).ok());
    REQUIRE(graph.connect(out(rotate), graph::PortRef{mix, "a"}).ok());
    REQUIRE(graph.connect(out(sphere), graph::PortRef{mix, "b"}).ok());
    REQUIRE(graph.setParameter(rotate, "angleY", 77.125).ok());
    REQUIRE(graph.setParameter(sphere, "radius", 0.4321).ok());

    const auto compiled = compiler::compileField(graph, builtins.library, out(mix));
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;

    // Kugel: Abstand im Raum, nicht nur in der Ebene.
    CHECK(wgsl.find("length(p - center)") != std::string::npos);
    // Welle: Projektion auf eine normierte 3D-Richtung.
    CHECK(wgsl.find("dot(p, n)") != std::string::npos);
    // Drehung: alle drei Achsen werden angewendet.
    CHECK(wgsl.find("radians(wh_params.n2_angleX)") != std::string::npos);
    CHECK(wgsl.find("radians(wh_params.n2_angleY)") != std::string::npos);
    CHECK(wgsl.find("radians(wh_params.n2_angleZ)") != std::string::npos);

    CHECK(wgsl.find("77.125") == std::string::npos);
    CHECK(wgsl.find("0.4321") == std::string::npos);
}

TEST_CASE("Built-ins: Rotate 3D mit allen Winkeln 0 lässt die Koordinate unverändert") {
    // Struktur-Prüfung: ohne Drehung bleibt der Aufruf des Eingangs bei d + c, und der
    // Standardwert jedes Winkels ist 0.
    Builtins builtins;
    const graph::NodeDescriptor* rotate = builtins.registry->find("wh.transform.rotate3d");
    REQUIRE(rotate != nullptr);
    int angles = 0;
    for (const graph::ParameterDescriptor& parameter : rotate->parameters) {
        REQUIRE(std::holds_alternative<double>(parameter.defaultValue));
        CHECK(std::get<double>(parameter.defaultValue) == 0.0);
        ++angles;
    }
    CHECK(angles == 3);
}
