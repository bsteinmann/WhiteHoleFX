// parameter_resolve.hpp: Modulation von Parametern durch verbundene Werte.
//
// Warum es diese Datei gibt: Die Regel "ein Eingang mit demselben Schlüssel wie
// ein Parameter moduliert diesen Parameter" wird von mehreren Stellen gebraucht
// (Registry-Prüfung, Parameterpuffer des Compilers, CPU-Evaluator, später die
// GUI). Sie steht deshalb einmal hier, im Graph-Modul, das alle kennen.
//
// Die Regel:
//   Hat ein Node-Typ einen Eingang und einen Parameter mit gleichem Schlüssel,
//   ersetzt eine Verbindung an diesem Eingang den gespeicherten Parameterwert.
//   Der Portyp muss zum Parametertyp passen:
//     double -> Scalar, int64 -> Integer, bool -> Boolean, ColorRgba -> Color.
//   Ist der Eingang unverbunden oder liefert die Quelle (noch) nichts, gilt der
//   gespeicherte Wert. Der Shader merkt nichts davon: Er liest weiter denselben
//   Slot im Uniform-Puffer.
#pragma once

#include <optional>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/value_lookup.hpp"

namespace whitehole::graph {

/// Portyp, über den ein Parameter dieses Werttyps moduliert wird.
/// Nichts für Vec2, Vec3 und Text (dafür gibt es keinen Portyp).
[[nodiscard]] std::optional<PortType> modulationPortType(const ParameterValue& value) noexcept;

/// Passt der Wert zum Datentyp des Ports? Scalar: double, Integer: int64,
/// Boolean und Trigger: bool, Color: ColorRgba, Text: string. Alles andere
/// (Image, Field, ...) lässt sich nicht als Einzelwert darstellen.
[[nodiscard]] bool valueMatchesPortType(PortType type, const ParameterValue& value) noexcept;

/// Hängt für jeden modulierbaren Parameter einen optionalen Eingang mit
/// gleichem Schlüssel und Anzeigenamen an, sofern noch keiner existiert.
/// Spart Node-Autoren die doppelte Pflege von Parameter und Eingang.
void addModulationPorts(NodeDescriptor& descriptor);

/// Wandelt einen gelieferten Wert in den Typ des Parameters um:
///  - double:    double, int64 und bool; auf [minValue, maxValue] begrenzt
///  - int64:     nur int64; begrenzt
///  - bool:      nur bool
///  - ColorRgba: nur ColorRgba
/// Nicht endliche Zahlen und alles andere liefern nichts.
[[nodiscard]] std::optional<ParameterValue> convertForParameter(const ParameterValue& source,
                                                                const ParameterDescriptor& target);

/// Der Wert, den der Parameter im aktuellen Frame hat: der umgewandelte Wert der
/// verbundenen Quelle, sonst der gespeicherte Wert. Nichts, wenn der Node keinen
/// gespeicherten Wert hat und nichts Verwertbares ankommt. `values` darf nullptr
/// sein, dann gilt immer der gespeicherte Wert.
[[nodiscard]] std::optional<ParameterValue> resolveParameter(const Graph& graph,
                                                             const NodeInstance& node,
                                                             const ParameterDescriptor& parameter,
                                                             const IValueLookup* values);

}  // namespace whitehole::graph
