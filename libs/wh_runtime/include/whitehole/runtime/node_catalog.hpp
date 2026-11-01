// node_catalog.hpp: Verzeichnis der Node-Typen als Text (für die Kommandozeile und die Dokumentation).
//
// Warum es das gibt: Wer eine Projektdatei von Hand schreibt, braucht die Typ-IDs, die Schlüssel der Ports und Parameter,
// Standardwerte, Bereiche und die Namen der Auswahlfelder. Bisher stand das nur im Quelltext. `whitehole_cli
// --list-nodes` und `--describe-node <typ>` geben es aus; die Funktionen hier erzeugen den Text, damit er ohne
// Programmstart testbar ist.
#pragma once

#include <string>
#include <string_view>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"

namespace whitehole::runtime {

/// Eine Zeile je Node-Typ: "typ-id  Kategorie  Anzeigename", nach Kategorie und Namen sortiert.
[[nodiscard]] std::string listNodes(const graph::INodeRegistry& registry);

/// Alle Ports und Parameter eines Typs. Fehler: NotFound mit Vorschlägen (Typen, die den Text enthalten).
[[nodiscard]] Result<std::string> describeNode(const graph::INodeRegistry& registry, std::string_view typeId);

/// Der Name eines Porttyps ("Field", "Scalar" …).
[[nodiscard]] const char* portTypeName(graph::PortType type) noexcept;

}  // namespace whitehole::runtime
