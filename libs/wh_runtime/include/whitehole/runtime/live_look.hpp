// live_look.hpp: Looks anlegen und in einen Graphen einsetzen.
//
// Warum es diese Datei gibt: Ein "Look" ist ein fertiger Effekt, vom Generator über die Modifier bis zum
// Ausgang, den man für den Live-Betrieb in einer Liste ablegt und per Knopfdruck abruft. Dieser Teil
// kann zwei Dinge, beide ohne Qt und ohne GPU:
//   - einen Look aus einem Graphen herauslösen (nur die Nodes, die den Ausgang speisen; Experimente
//     daneben bleiben draußen),
//   - einen Look in einen anderen Graphen einsetzen (Live-Decks, Editor).
// Die Look-Daten selbst (project::Look) liegen in wh_project, weil sie zur Projektdatei gehören.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

/// Ein Name taugt für die Look-Liste: nicht leer, höchstens 64 Zeichen, keine Steuerzeichen.
[[nodiscard]] bool isValidLookName(std::string_view name);

/// Löst den Effekt hinter `target` aus `graph`: den Ausgangs-Node und alles, was ihn über Kabel speist
/// (auch Werte-Nodes wie LFO oder Clock). Fehler: InvalidArgument (Name, Überblendzeit außerhalb von
/// 0..kMaxFadeSeconds), NotFound (Ziel nicht im Graphen), sonst die der Graph-Operationen.
[[nodiscard]] Result<project::Look> captureLook(const graph::Graph& graph, const graph::PortRef& target,
                                                std::string name, double fadeSeconds);

/// Was beim Einsetzen eines Looks entsteht.
struct PlacedLook {
    std::vector<NodeId> nodes;  // alle neuen Nodes, in der Reihenfolge des Look-Graphen
    graph::PortRef output;      // der Ausgang des Looks im Zielgraphen
};

/// Legt die Nodes des Looks samt Parametern und Kabeln in `graph` an; `offset` kommt auf alle Positionen.
/// Scheitert ein Schritt, kann `graph` halb gefüllt zurückbleiben (Aufrufer sichert oder wirft weg).
[[nodiscard]] Result<PlacedLook> placeLook(graph::Graph& graph, const project::Look& look, Vec2 offset);

/// Index des Looks mit diesem Namen, oder -1.
[[nodiscard]] int findLook(const std::vector<project::Look>& looks, std::string_view name) noexcept;

}  // namespace whitehole::runtime
