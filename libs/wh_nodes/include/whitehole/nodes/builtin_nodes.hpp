// builtin_nodes.hpp: die eingebauten Nodes (Field-Nodes und Werte-Nodes).
#pragma once

#include <vector>

#include "whitehole/compiler/node_definition.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"
#include "whitehole/values/cpu_kernel.hpp"

namespace whitehole::nodes {

/// Field-Nodes (Snippets, laufen auf der GPU). Typ-IDs: Schema "wh.<kategorie>.<name>"
///   wh.generator.gradient, wh.generator.noise, wh.generator.plasma
///   wh.transform.rotate,   wh.transform.mirror, wh.transform.scale
///   wh.blend.mix,          wh.blend.add
/// Jeder Parameter, der sich modulieren lässt, hat einen Eingang gleichen Schlüssels.
[[nodiscard]] std::vector<compiler::NodeDefinition> builtinNodeDefinitions();

/// Werte-Nodes (Kernel, laufen auf der CPU).
///   wh.value.time, wh.value.number, wh.value.color, wh.value.lfo,
///   wh.value.random, wh.value.map, wh.value.smooth
[[nodiscard]] std::vector<values::CpuNodeDefinition> builtinCpuDefinitions();

/// Trägt alle eingebauten Nodes samt Snippets und Kerneln ein.
/// Fehler: AlreadyExists, wenn sie schon registriert sind.
Result<void> registerBuiltinNodes(graph::INodeRegistrar& registrar, compiler::SnippetLibrary& snippets,
                                  values::CpuKernelLibrary& kernels);

}  // namespace whitehole::nodes
