// node_definition.hpp: Node-Beschreibung plus Shader-Snippet.
#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_descriptor.hpp"
#include "whitehole/graph/node_registry.hpp"

namespace whitehole::compiler {

/// WGSL-Code eines Field-Nodes.
///
/// `body` ist der Rumpf von `fn wh_f<ID>(p: vec3<f32>) -> vec4<f32>` und muss
/// mit `return` enden. Platzhalter (siehe snippet_expander.hpp):
///   @in(port, koordinate)   Wert des Field-Eingangs an der Koordinate
///   @param(schluessel)      Parameterwert des Nodes (liegt im Uniform-Puffer)
///   @time                   Zeit des aktuellen Frames in Sekunden
/// Der Name `p` ist belegt; lokale Variablen dürfen nicht so heißen.
///
/// `prelude` enthält optionale Hilfsfunktionen, die einmal vor allen Node-
/// Funktionen stehen. Identische Preludes mehrerer Nodes werden zusammengelegt
/// und nicht expandiert (hier dürfen auch WGSL-Attribute mit '@' stehen).
struct NodeSnippet {
    std::string prelude;
    std::string body;
};

struct NodeDefinition {
    graph::NodeDescriptor descriptor;
    NodeSnippet snippet;
};

class ISnippetLibrary {
public:
    virtual ~ISnippetLibrary() = default;

    /// Liefert nullptr, wenn für den Typ kein Snippet existiert.
    [[nodiscard]] virtual const NodeSnippet* find(std::string_view typeId) const = 0;
};

class SnippetLibrary final : public ISnippetLibrary {
public:
    [[nodiscard]] const NodeSnippet* find(std::string_view typeId) const override;

    /// Fehler: AlreadyExists, wenn für die Typ-ID schon ein Snippet vorliegt.
    Result<void> add(std::string typeId, NodeSnippet snippet);

private:
    std::map<std::string, NodeSnippet, std::less<>> snippets_;
};

/// Trägt Beschreibung und Snippet jeder Definition ein (Beschreibung zuerst).
/// Bricht beim ersten Fehler ab; bereits eingetragene Definitionen bleiben
/// bestehen, das Laden einer Plugin-Gruppe ist also nicht atomar.
Result<void> registerDefinitions(std::span<const NodeDefinition> definitions,
                                 graph::INodeRegistrar& registrar, SnippetLibrary& library);

}  // namespace whitehole::compiler
