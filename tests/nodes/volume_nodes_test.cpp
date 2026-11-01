// volume_nodes_test.cpp: die 3D-Regler der Nodes und die neuen Volumen-Nodes (Descriptor-Ebene).
//
// Warum es den Test gibt: Die z-Regler der älteren Nodes (elevation, depth, factorZ, countZ ...) sind
// nachträglich dazugekommen. Alte Projekte kennen sie nicht und bekommen den Standardwert; der muss das
// bisherige 2D-Bild unverändert lassen. Diese Datei hält die neutralen Standardwerte fest. Ob der Shader
// dann wirklich dasselbe Bild liefert, prüfen die GPU-Tests auf der CI (tests/gpu_device).
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

#include "whitehole/nodes/builtin_nodes.hpp"

using namespace whitehole;

namespace {

const graph::ParameterDescriptor* find(const std::vector<compiler::NodeDefinition>& all, const std::string& typeId,
                                       const std::string& key) {
    for (const compiler::NodeDefinition& definition : all) {
        if (definition.descriptor.typeId == typeId) {
            return definition.descriptor.findParameter(key);
        }
    }
    return nullptr;
}

double numberOf(const graph::ParameterDescriptor& parameter) {
    if (const auto* real = std::get_if<double>(&parameter.defaultValue)) {
        return *real;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&parameter.defaultValue)) {
        return static_cast<double>(*integer);
    }
    FAIL("Parameter ist keine Zahl: " << parameter.key);
    return 0.0;
}

}  // namespace

TEST_CASE("3D-Regler älterer Nodes haben neutrale Standardwerte (2D-Bild bleibt gleich)", "[nodes][volume]") {
    const auto all = nodes::builtinNodeDefinitions();
    struct Expect {
        const char* typeId;
        const char* key;
        double value;
    };
    const Expect expected[] = {
        {"wh.generator.gradient", "elevation", 0.0}, {"wh.generator.scan", "elevation", 0.0},
        {"wh.generator.plasma", "depth", 0.0},       {"wh.generator.fire", "depth", 0.0},
        {"wh.generator.ripples", "space", 0.0},      {"wh.generator.metaballs", "space", 0.0},
        {"wh.generator.starfield", "space", 0.0},    {"wh.generator.fading", "space", 0.0},
        {"wh.generator.shapes", "space", 0.0},       {"wh.generator.sinelines", "frequencyZ", 0.0},
        {"wh.transform.scale", "factorZ", 1.0},      {"wh.transform.slide", "z", 0.0},
        {"wh.transform.tile", "countZ", 1.0},        {"wh.transform.pixelate", "cellsZ", 0.0},
        {"wh.transform.region", "zStart", 0.0},      {"wh.transform.region", "zSize", 1.0},
    };
    for (const Expect& e : expected) {
        INFO(e.typeId << "." << e.key);
        const graph::ParameterDescriptor* parameter = find(all, e.typeId, e.key);
        REQUIRE(parameter != nullptr);
        CHECK(numberOf(*parameter) == e.value);
    }
}

TEST_CASE("Die 3D-Auswahlfelder bieten Flat und Volume an", "[nodes][volume]") {
    const auto all = nodes::builtinNodeDefinitions();
    for (const char* typeId : {"wh.generator.ripples", "wh.generator.metaballs", "wh.generator.starfield",
                               "wh.generator.fading", "wh.generator.shapes"}) {
        INFO(typeId);
        const graph::ParameterDescriptor* parameter = find(all, typeId, "space");
        REQUIRE(parameter != nullptr);
        REQUIRE(parameter->choices.size() == 2U);
        CHECK(parameter->choices[0].rfind("Flat", 0) == 0U);
        CHECK(parameter->choices[1].rfind("Volume", 0) == 0U);
    }
}

TEST_CASE("Neue Volumen-Nodes: Box, Helix, Position Test und Twist", "[nodes][volume]") {
    const auto all = nodes::builtinNodeDefinitions();
    for (const auto& [typeId, key] : {std::pair{"wh.generator.box", "sizeZ"}, std::pair{"wh.generator.helix", "pitch"},
                                      std::pair{"wh.generator.position", "gain"}, std::pair{"wh.transform.twist", "angle"}}) {
        INFO(typeId);
        CHECK(find(all, typeId, key) != nullptr);
    }
    // Der Twist dreht standardmäßig um die senkrechte Achse (Auswahl "Y").
    const graph::ParameterDescriptor* axis = find(all, "wh.transform.twist", "axis");
    REQUIRE(axis != nullptr);
    CHECK(numberOf(*axis) == 1.0);
    CHECK(axis->choices.size() == 3U);
}
