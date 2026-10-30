// value_lookup.hpp: Schnittstelle, über die Compiler und Evaluator aktuelle
// Werte von Ausgängen erfragen.
//
// Warum es diese Datei gibt: Der Feldcompiler muss beim Befüllen des Parameter-
// puffers wissen, was ein verbundener CPU-Node gerade liefert, soll aber weder
// den Evaluator noch dessen Kernel kennen. Diese kleine Schnittstelle trennt
// beide (Dependency Inversion): wh_compiler hängt nur an wh_graph, die
// Implementierung (ValueStore) lebt in wh_values.
#pragma once

#include <optional>
#include <string_view>

#include "whitehole/core/ids.hpp"
#include "whitehole/graph/node_descriptor.hpp"

namespace whitehole::graph {

class IValueLookup {
public:
    virtual ~IValueLookup() = default;

    /// Der Wert, den der Ausgang `portKey` des Nodes im aktuellen Frame liefert,
    /// oder nichts, wenn er (noch) keinen geliefert hat.
    [[nodiscard]] virtual std::optional<ParameterValue> value(NodeId node,
                                                              std::string_view portKey) const = 0;
};

}  // namespace whitehole::graph
