// node_docking.hpp: plant, wohin ein neuer Node einrastet, wenn man ihn aus der Palette auf einen Node oder ein Kabel zieht.
//
// Warum es das gibt: Wer einen Node aus der Palette zieht, soll ihn nicht erst platzieren und dann von Hand verkabeln müssen.
// Fällt er auf ein Kabel, wird er dazwischen eingefügt (A -> neu -> B); fällt er auf einen Node, wird er mit ihm verbunden.
// Die Regeln stehen hier als reine Funktion über dem Graphen und nicht im Fenster, damit sie ohne Oberfläche getestet
// werden (tests/graph/node_docking_test.cpp). Der Editor zeigt den Plan als Vorschau; das Ausführen in einem
// Rückgängig-Schritt übernimmt Player::addNodeDocked.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::graph {

/// Worauf der neue Node fällt.
struct DockTarget {
    enum class Kind { Wire, Node };
    Kind kind{Kind::Node};
    ConnectionId wire;  // Kind::Wire: das Kabel, in das der Node eingefügt wird
    NodeId node;        // Kind::Node: der getroffene Node
    /// Kind::Node: Fiel der Mauszeiger auf die rechte Hälfte, hängt der neue Node hinter den Zielnode (dessen Ausgang
    /// speist ihn); links speist der neue Node einen Eingang des Zielnodes.
    bool rightHalf{false};
    /// Kind::Node: Port des Zielnodes, den die Mausposition gewählt hat (Schlüssel). Leer oder unpassend = automatisch.
    std::string preferredPort;
};

/// Eine Verbindung des Plans. Ein ungültiger NodeId{} an `from` oder `to` steht für den neuen Node, der noch keine Id hat.
struct DockLink {
    PortRef from;
    PortRef to;
};

struct DockPlan {
    /// Bestehende Kabel, die vorher gelöst werden (beim Einfügen das durchtrennte Kabel).
    std::vector<ConnectionId> remove;
    /// Neue Kabel, in dieser Reihenfolge anzulegen.
    std::vector<DockLink> links;
};

/// Plant das Einrasten eines Nodes vom Typ `newNode` am Ziel. Nichts, wenn nichts passt (dann legt der Editor den Node
/// einfach ab). Der Plan ändert den Graphen nicht und ist immer zyklenfrei, weil der neue Node noch keine Kabel hat.
///
/// Regeln: Bei mehreren Eingängen gilt der erste freie Eingang mit dem passenden Typ, sonst der erste passende. Ist der
/// gewählte Eingang des Zielnodes belegt, wird der neue Node in dieses Kabel eingefügt (verträgt sein Eingang den bisherigen
/// Ausgang nicht, rastet nichts ein, damit kein Kabel verloren geht).
[[nodiscard]] std::optional<DockPlan> planDocking(const Graph& graph, const NodeDescriptor& newNode, const DockTarget& target);

}  // namespace whitehole::graph
