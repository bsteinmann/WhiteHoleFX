// builtin_nodes.hpp: die eingebauten Nodes (Field-Nodes und Werte-Nodes).
#pragma once

#include <memory>
#include <vector>

#include "whitehole/compiler/node_definition.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"
#include "whitehole/audio/audio_store.hpp"
#include "whitehole/input/dmx_store.hpp"
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
/// DMX-Eingang: wh.value.dmx, wh.value.dmxcolor; sie lesen aus `dmx` (null = leerer Speicher, kein Signal).
/// Audio-Eingang: wh.value.audiolevel, audioband, audiospectrum, beat, tap (Kategorie "Audio"); sie lesen
/// aus `audio` (null = leerer Speicher, kein Signal). Dazu der Field-Node wh.generator.spectrum.
[[nodiscard]] std::vector<values::CpuNodeDefinition> builtinCpuDefinitions(std::shared_ptr<input::DmxStore> dmx = nullptr,
                                                                           std::shared_ptr<audio::AudioStore> audio = nullptr);

/// Trägt alle eingebauten Nodes samt Snippets und Kerneln ein.
/// Fehler: AlreadyExists, wenn sie schon registriert sind.
Result<void> registerBuiltinNodes(graph::INodeRegistrar& registrar, compiler::SnippetLibrary& snippets,
                                  values::CpuKernelLibrary& kernels, std::shared_ptr<input::DmxStore> dmx = nullptr,
                                  std::shared_ptr<audio::AudioStore> audio = nullptr);

}  // namespace whitehole::nodes
