// node_group.hpp: Eine Gruppe von Nodes mit selbst gewählter Schnittstelle.
//
// Warum es das gibt: Ein bewährter Aufbau (Plasma mit Farbwechsel und Blur) besteht aus vielen Nodes, die im
// Editor Platz und Aufmerksamkeit brauchen. Eine Gruppe fasst sie zu einer Kachel zusammen, die nur zeigt, was man
// von außen braucht: ausgewählte Ein- und Ausgänge und ausgewählte Parameter als Regler.
//
// Eine Gruppe ist nur ein Behälter mit Beschriftung: Die Nodes bleiben ganz normale Nodes im Graphen, die Kabel
// bleiben normale Kabel, und der Compiler sieht von Gruppen nichts. Darum kann sie den Rechenkern nicht beeinflussen,
// und Rückgängig, Kopieren und Speichern funktionieren wie bei allen Nodes. Die Gruppe legt nur fest, was die
// Oberfläche zeigt (Kachel oder aufgeklappt) und was als Schnittstelle gilt.
//
// Eine Gruppe gehört zu einem Graphen; ein Node steckt in höchstens einer Gruppe (kein Verschachteln).
#pragma once

#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/types.hpp"

namespace whitehole::graph {

/// Ein nach außen gezeigter Port eines Mitglieds (Eingang oder Ausgang, das sagt der Node-Typ).
struct GroupPort {
    NodeId node;
    std::string portKey;
    std::string label;  // Beschriftung auf der Kachel

    bool operator==(const GroupPort&) const = default;
};

/// Ein nach außen gezeigter Parameter eines Mitglieds, der als Regler der Gruppe erscheint.
struct GroupParameter {
    NodeId node;
    std::string key;
    std::string label;

    bool operator==(const GroupParameter&) const = default;
};

inline constexpr std::size_t kMaxGroupNameLength = 64;

struct NodeGroup {
    std::string name;  // eindeutig im Graphen
    Vec2 position;     // Lage der Kachel im Node-Editor
    Vec3 color{0.25F, 0.45F, 0.90F};
    bool collapsed{true};  // zugeklappt = nur die Kachel, aufgeklappt = die Mitglieder einzeln
    std::vector<NodeId> members;
    std::vector<GroupPort> ports;
    std::vector<GroupParameter> parameters;
};

}  // namespace whitehole::graph
