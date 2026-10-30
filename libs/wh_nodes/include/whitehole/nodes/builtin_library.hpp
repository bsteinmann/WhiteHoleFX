// builtin_library.hpp: alle eingebauten Nodes fertig verdrahtet.
//
// Warum es diese Datei gibt: Wer ein Projekt lädt und rendert (Konsolen-Runner, später die GUI),
// braucht drei zusammengehörige Dinge: das Verzeichnis der Node-Typen für den Graphen, die
// Shader-Bausteine für den Compiler und die CPU-Rechenkerne für die Werte-Nodes. Dieses Bündel
// legt sie einmal an, damit niemand die Verdrahtung wiederholt.
#pragma once

#include <memory>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/values/cpu_kernel.hpp"

namespace whitehole::nodes {

struct BuiltinLibrary {
    std::shared_ptr<graph::NodeRegistry> registry;
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;

    /// Fehler: die der Registrierung (z. B. doppelte Typ-ID, die es bei Built-ins nicht geben darf).
    [[nodiscard]] static Result<std::unique_ptr<BuiltinLibrary>> create();
};

}  // namespace whitehole::nodes
