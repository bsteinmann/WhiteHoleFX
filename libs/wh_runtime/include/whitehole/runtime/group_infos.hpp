// group_infos.hpp: Parameter von Gruppen für die Parameteranzeige.
//
// Warum es das gibt: Eine zugeklappte Gruppe (graph/node_group.hpp) zeigt im Parameterdock nur die Regler, die der Nutzer
// nach außen geführt hat, unter dem Namen der Gruppe und mit eigener Beschriftung. Das Dock kennt Gruppen nicht:
// `applyGroups` macht aus der Liste der Nodes (Player::nodes) eine Liste, in der die Mitglieder zugeklappter Gruppen
// durch einen Eintrag je Gruppe ersetzt sind. Dessen Schlüssel kodieren Node und Parameter ("@12/scale"), damit der
// Änderungsauftrag des Docks den echten Parameter findet (`parseGroupParameterKey`).
//
// Reine Datenarbeit ohne Qt, daher testbar. Aufgeklappte Gruppen ändern nichts: ihre Nodes sind ganz normale Nodes.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

/// Schlüssel eines Gruppen-Parameters in der Anzeige: "@<NodeId>/<Schlüssel>".
[[nodiscard]] std::string groupParameterKey(NodeId node, const std::string& key);
/// Gegenstück; leer, wenn `key` kein Gruppen-Schlüssel ist.
[[nodiscard]] std::optional<std::pair<NodeId, std::string>> parseGroupParameterKey(std::string_view key);

/// Ersetzt die Einträge der Mitglieder zugeklappter Gruppen durch einen Eintrag je Gruppe (Id des ersten Mitglieds,
/// Titel = Gruppenname, Typ "group", Parameter = die nach außen geführten, mit Beschriftung der Gruppe). Der Eintrag
/// steht an der Stelle des ersten Mitglieds. Nodes anderer Gruppen und ohne Gruppe bleiben unverändert.
[[nodiscard]] std::vector<NodeInfo> applyGroups(const graph::Graph& graph, std::vector<NodeInfo> nodes);

}  // namespace whitehole::runtime
