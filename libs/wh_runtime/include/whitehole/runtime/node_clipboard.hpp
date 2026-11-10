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
#include "whitehole/core/result.hpp"
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
    double scale{1.0};   // Skalierung am Kabel (Connection::scale)
    double offset{0.0};
};

/// Eine Gruppe (graph/node_group.hpp) in der Zwischenablage: Mitglieder, Ports und Parameter als Indizes in `nodes`.
/// Kopiert werden nur Gruppen, deren Mitglieder alle dabei sind.
struct ClipboardGroup {
    std::string name;
    Vec2 position;
    Vec3 color;
    bool collapsed{true};
    std::vector<std::size_t> members;
    struct Port {
        std::size_t node{0};
        std::string key;
        std::string label;
    };
    std::vector<Port> ports;
    std::vector<Port> parameters;
};

struct NodeClipboard {
    std::vector<ClipboardNode> nodes;
    std::vector<ClipboardWire> wires;
    std::vector<ClipboardGroup> groups;

    [[nodiscard]] bool empty() const noexcept { return nodes.empty(); }
    /// Linke obere Ecke aller kopierten Nodes (für das Einfügen an einer Stelle).
    [[nodiscard]] Vec2 topLeft() const noexcept;
};

/// Kopiert die Nodes aus `selection` (unbekannte Ids werden übersprungen) in der Reihenfolge des
/// Graphen, dazu alle Kabel, deren beide Enden in der Auswahl liegen.
[[nodiscard]] NodeClipboard copyNodes(const graph::Graph& graph, std::span<const NodeId> selection);

/// Legt die Nodes der Zwischenablage im Graphen an (Parameter, Kabel und Gruppen inklusive); `offset` wird auf
/// ihre Positionen addiert. Eine Gruppe, deren Name im Graphen schon vergeben ist, bekommt eine Zahl angehängt
/// ("Name 2"). Liefert die neuen Ids in der Reihenfolge der Zwischenablage. Scheitert
/// ein Schritt, bleibt der Graph möglicherweise halb gefüllt: Der Aufrufer muss ihn zurücksetzen
/// (der Player sichert dafür das Projekt, ein Preset-Export arbeitet auf einem Wegwerf-Graphen).
/// Fehler: die von Graph::addNode, setParameter und connect, InvalidArgument bei beschädigten Indizes.
[[nodiscard]] Result<std::vector<NodeId>> applyClipboard(graph::Graph& graph, const NodeClipboard& clipboard,
                                                         Vec2 offset);

}  // namespace whitehole::runtime
