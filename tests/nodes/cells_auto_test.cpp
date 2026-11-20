// Zellenzahl automatisch (cells = 0): Standardwerte der vier Nodes mit einem cells-Parameter. Warum eine eigene Datei:
// builtin_nodes_test.cpp ist schon über der Grenze von etwa 400 Zeilen. Das Bild selbst prüft
// tests/gpu_device/cells_auto_on_device_test.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <variant>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
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

}  // namespace

TEST_CASE("Built-ins: cells-Parameter sind standardmaessig 0 (automatisch aus der echten Auflösung)") {
    Builtins builtins;
    for (const char* type : {"wh.generator.maze", "wh.generator.starfield", "wh.generator.fading", "wh.generator1d.sparkle"}) {
        INFO(type);
        const graph::NodeDescriptor* descriptor = builtins.registry->find(type);
        REQUIRE(descriptor != nullptr);
        const graph::ParameterDescriptor* cells = nullptr;
        for (const graph::ParameterDescriptor& candidate : descriptor->parameters) {
            if (candidate.key == "cells") {
                cells = &candidate;
            }
        }
        REQUIRE(cells != nullptr);
        CHECK(cells->minValue == 0.0);  // 0 = automatisch (@columns/@rows), > 0 = manueller Override
        if (const auto* integerDefault = std::get_if<std::int64_t>(&cells->defaultValue)) {
            CHECK(*integerDefault == 0);
        } else {
            CHECK(std::get<double>(cells->defaultValue) == 0.0);
        }
    }
}
