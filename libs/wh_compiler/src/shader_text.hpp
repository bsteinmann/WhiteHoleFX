// shader_text.hpp: Hilfen für den erzeugten Shader-Text (intern im Feldcompiler).
//
// Warum es diese Datei gibt: Der Name einer Node-Funktion (wh_f…) und eines Parameters (wh_params.n…_key) im Shader
// darf nicht an der Node-Nummer hängen. Sonst ergibt dieselbe Struktur mit anderen Nummern einen anderen Text und damit
// einen anderen structureHash: Im Live-Betrieb setzt jedes Vorladen die Nodes eines Looks neu ein (neue Nummern), der
// Pipeline-Zwischenspeicher traf deshalb nie, und jedes Drücken und Loslassen eines Flash-Looks ließ den Treiber neu
// übersetzen (Rückmeldung vom 26.11.: Flash-Looks stocken deutlich). Darum schreibt der Compiler beim Erzeugen nur
// Platzhalter mit der Node-Nummer; resolveShaderNames ersetzt sie am Ende durch die Stelle der Node in der
// Ausgabereihenfolge (1, 2, 3 …). Das Parameter-Layout behält die echten Node-Nummern: packParameters liest die Werte
// weiter aus dem Graphen, nur die Namen im Text sind neu nummeriert.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "whitehole/compiler/parameter_layout.hpp"
#include "whitehole/core/ids.hpp"

namespace whitehole::compiler::detail {

/// Platzhalter für den Namen der Funktion einer Node: "wh_f" + Platzhalter.
[[nodiscard]] std::string functionName(NodeId id);

/// Platzhalter für den Namen eines Parameters im Uniform-Struct (ohne "wh_params.").
[[nodiscard]] std::string memberRef(NodeId id, std::string_view key);

/// Ersetzt alle Platzhalter in `wgsl` durch die Stelle der Node in `order` (ab 1) und benennt die Slots des
/// Layouts gleich. Eine Node, die nicht in `order` steht, behält ihre Nummer (sollte nicht vorkommen).
void resolveShaderNames(std::string& wgsl, ParameterLayout& layout, const std::vector<NodeId>& order);

/// Rückt jede nichtleere Zeile um vier Leerzeichen ein und schließt mit Zeilenumbruch ab.
[[nodiscard]] std::string indent(const std::string& text);

/// "@group(…) @binding(n) var<uniform> declaration;"
[[nodiscard]] std::string bindingLine(std::uint32_t binding, const std::string& declaration);

}  // namespace whitehole::compiler::detail
