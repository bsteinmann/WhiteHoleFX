// node_clipboard.hpp: Zwischenablage für Nodes (Kopieren, Einfügen, Duplizieren, später Presets).
//
// Warum es das gibt: Wer einen Teil des Graphen mehrfach braucht, soll ihn nicht von Hand neu
// bauen. Die Zwischenablage hält ausgewählte Nodes samt Parametern und die Kabel, die zwischen
// ihnen laufen. Kabel von oder nach draußen fallen weg (sie hängen an Nodes, die nicht dabei sind).
// Das ist reine Daten- und Rechenarbeit auf dem Graphen, ohne Qt, und lässt sich deshalb testen.
// Eingefügt wird über Player::pasteNodes, als ein Schritt für Rückgängig.
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::runtime {

struct ClipboardNode {
    std::string typeId;
    Vec2 position;  // wie im Quellgraphen; beim Einfügen kommt ein Versatz dazu
    std::vector<std::pair<std::string, graph::ParameterValue>> parameters;  // nach Schlüssel sortiert
};

struct ClipboardWire {
    std::size_t from{0};  // Index in NodeClipboard::nodes
    std::string fromPort;
    std::size_t to{0};
    std::string toPort;
};

struct NodeClipboard {
    std::vector<ClipboardNode> nodes;
    std::vector<ClipboardWire> wires;

    [[nodiscard]] bool empty() const noexcept { return nodes.empty(); }
    /// Linke obere Ecke aller kopierten Nodes (für das Einfügen an einer Stelle).
    [[nodiscard]] Vec2 topLeft() const noexcept;
};

/// Kopiert die Nodes aus `selection` (unbekannte Ids werden übersprungen) in der Reihenfolge des
/// Graphen, dazu alle Kabel, deren beide Enden in der Auswahl liegen.
[[nodiscard]] NodeClipboard copyNodes(const graph::Graph& graph, std::span<const NodeId> selection);

}  // namespace whitehole::runtime
