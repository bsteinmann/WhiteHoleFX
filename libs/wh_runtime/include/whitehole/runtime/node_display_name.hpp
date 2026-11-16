// node_display_name.hpp: der Name, den ein Node in der Oberfläche, im Parameter-Dock oder in einem
// Rechtsklick-Menü trägt.
//
// Warum es das gibt: NodeInstance::label ist der EIGENE Name, den der Nutzer per Player::renameNode
// gesetzt hat - meist leer. Jede anzeigende Stelle braucht denselben Automatiknamen: den Anzeigenamen des
// Node-Typs, durchnummeriert, falls mehrere Nodes desselben Typs im Graphen stehen ("DMX Channel", "DMX
// Channel 2", ...) - exakt dieselbe Zählung, die es schon vor der Einführung eigener Namen an drei Stellen
// gab (Player::nodes() fürs Parameter-Dock, NodeEditor::buildScene() für die Node-Titel, dmx_listener.cpp
// fürs "Node"-Menü im DMX-Monitor). Diese Funktion bündelt die Zählung an einer Stelle, damit sie überall
// gleich bleibt.
//
// Eigener Name zusätzlich, nicht statt des Automatiknamens (vom Nutzer gewünscht): mit eigenem Namen lautet
// das Ergebnis "Automatikname (Eigener Name)", z. B. "DMX Channel 2 (Eingangstür)" - so bleibt auf einen
// Blick sichtbar, was für ein Node das ist, auch mit eigenem Namen. Für den RAW-Namen allein (z. B. zum
// Vorbefüllen des Umbenennen-Dialogs) nicht diese Funktion verwenden, sondern direkt
// `graph.findNode(id)->label`.
//
// Bewusst wird über ALLE Nodes eines Typs gezählt, nicht nur die ohne eigenen Namen: Ein Node, der (noch)
// keinen eigenen Namen hat, soll seine Nummer nicht ändern, nur weil ein anderer Node desselben Typs gerade
// umbenannt wurde - das wäre eine überraschende Nebenwirkung des Umbenennens.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "whitehole/core/ids.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::runtime {

/// Der Anzeigename jedes Nodes im Graphen (siehe Kommentar oben), als NodeId -> Name. Ein Node mit
/// unbekanntem Typ (Registry kennt ihn nicht) fehlt im Ergebnis.
[[nodiscard]] std::unordered_map<std::uint64_t, std::string> nodeDisplayNames(const graph::Graph& graph);

/// Dieselbe Berechnung für genau einen Node (etwas teurer, da sie intern über den ganzen Graphen zählt -
/// für einen einzelnen Treffer in Ordnung, in einer Schleife über viele Nodes lieber nodeDisplayNames()
/// einmal aufrufen). Leer, wenn der Node nicht existiert oder sein Typ unbekannt ist.
[[nodiscard]] std::string nodeDisplayName(const graph::Graph& graph, NodeId id);

}  // namespace whitehole::runtime
