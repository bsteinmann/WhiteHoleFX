// text_bindings.hpp: die Platzhalter @text und @textlen des Feldcompilers (intern).
//
// Warum es diese Datei gibt: field_compiler.cpp war über die 400 Zeilen hinausgewachsen. Textparameter sind der
// abgeschlossenste Teil der Platzhalter-Auflösung (kein Uniform-Slot, keine Verbindung zum Rest des Compilers), darum
// wohnen sie hier: Ein Text wird beim Kompilieren als Zeichenfeld in den Shader geschrieben; ändert er sich, ändert
// sich die Struktur und die Pipeline wird neu gebaut.
#pragma once

#include "whitehole/compiler/snippet_expander.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::compiler::detail {

/// Setzt `bindings.text` und `bindings.textLength` für die Textparameter von `node`. `node` und `descriptor` müssen
/// so lange leben wie `bindings`.
void bindTextPlaceholders(SnippetBindings& bindings, const graph::NodeInstance& node,
                          const graph::NodeDescriptor& descriptor);

}  // namespace whitehole::compiler::detail
