// project_nodegroup.hpp (privat): Gruppen von Nodes (`nodegroup`-Block) lesen und schreiben.
//
// Warum eine eigene Datei: project_parser.cpp und project_writer.cpp sind am Limit der Dateigröße, und der Block hat eine
// eigene kleine Grammatik. Beide Richtungen stehen hier nebeneinander, damit sie zusammenpassen.
//
//   nodegroup "Name"
//     at <x> <y>                      Lage der Kachel
//     color <r> <g> <b>               Farbe (0..1)
//     expanded                        aufgeklappt (ohne die Zeile: zugeklappt)
//     member <node> [<node> ...]      die Nodes der Gruppe (Namen aus den node-Blöcken)
//     port <node>.<port> as "Text"    nach außen gezeigter Ein- oder Ausgang
//     param <node>.<parameter> as "Text"   nach außen gezeigter Regler
//   end
//
// Der Block steht hinter den node- und connect-Zeilen seines Graphen (Hauptgraph oder look), weil er Nodes beim Namen nennt.
#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "project_text.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::project::detail {

/// Was der Block bisher sagt, mit Namen statt Node-Nummern. Erst `finishNodeGroup` löst die Namen auf.
struct NodeGroupDraft {
    struct Ref {
        std::string node;
        std::string key;
        std::string label;
    };
    std::string name;
    Vec2 position{};
    Vec3 color{0.25F, 0.45F, 0.90F};
    bool collapsed{true};
    std::vector<std::string> members;
    std::vector<Ref> ports;
    std::vector<Ref> parameters;
};

/// Liest die `nodegroup`-Zeile ab dem Wort nach "nodegroup". Fehler stehen im Reader.
[[nodiscard]] NodeGroupDraft beginNodeGroup(Reader& r);
/// Eine Zeile im Block (ab dem Wort nach dem Schlüsselwort). Fehler stehen im Reader.
void nodeGroupStatement(std::string_view key, Reader& r, NodeGroupDraft& draft);
/// Löst die Namen auf und legt die Gruppe im Graphen an (mit allen Prüfungen von Graph::addGroup).
[[nodiscard]] Result<void> finishNodeGroup(const NodeGroupDraft& draft, const std::unordered_map<std::string, NodeId>& nodes,
                                           graph::Graph& graph);

/// Schreibt alle Gruppen des Graphen; `names` übersetzt die Node-Nummern in die Namen der node-Blöcke.
void writeNodeGroups(std::ostream& out, const graph::Graph& graph,
                     const std::unordered_map<std::uint64_t, std::string>& names);

}  // namespace whitehole::project::detail
