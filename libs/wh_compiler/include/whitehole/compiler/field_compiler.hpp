// field_compiler.hpp: verschmilzt die Field-Nodes eines Graphen zu einer WGSL-Funktion.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "whitehole/compiler/node_definition.hpp"
#include "whitehole/compiler/parameter_layout.hpp"
#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/graph/graph_model.hpp"

namespace whitehole::compiler {

/// Die Feldbibliothek belegt im Shader diese Bindings in Bind-Group 0.
/// Konsumenten (Mapping-Pass, Canvas-Pass) verwenden Bind-Group 1 und höher.
inline constexpr std::uint32_t kFieldBindGroup = 0;
inline constexpr std::uint32_t kFrameBinding = 0;       // FrameUniforms
inline constexpr std::uint32_t kParametersBinding = 1;  // Puffer gemäß ParameterLayout

struct CompiledField {
    /// WGSL-Bibliothek: Bindings, Structs, Hilfsfunktionen, eine Funktion je Node
    /// und der Einstieg `fn wh_field(p: vec3<f32>) -> vec4<f32>`. Der Konsument
    /// hängt seinen Compute-Einstieg an und ruft wh_field() auf.
    /// Koordinaten: p in [0,1]^3, bei 2D-Canvas ist p.z = 0.
    std::string wgsl;

    ParameterLayout layout;

    /// FNV-1a des WGSL-Texts. Er hängt nur von der Struktur ab, nicht von
    /// Parameterwerten: Gleicher Hash bedeutet, dass die Pipeline weiterverwendet
    /// werden kann und nur der Parameterpuffer neu zu füllen ist.
    std::uint64_t structureHash{0};

    /// Beteiligte Nodes in Auswertungsreihenfolge (Eingänge zuerst).
    /// Nicht verbundene Nodes des Graphen kommen hier nicht vor.
    std::vector<NodeId> nodes;
};

/// Übersetzt alles, was von `target` (einem Field-Ausgang) aus rückwärts erreichbar ist.
///
/// Fehler:
///  - NotFound: Ziel-Node/-Port oder ein Snippet fehlt.
///  - InvalidArgument: Ziel ist kein Field-Ausgang; Pflicht-Eingang unverbunden;
///    Snippet verweist auf unbekannte Ports oder Parameter.
///  - Unsupported: Node außerhalb der Field-Domäne, Node mit mehreren
///    Field-Ausgängen, verbundener Nicht-Field-Eingang ohne gleichnamigen
///    Parameter, Bild als Field-Eingang (folgt mit den Raster-Pässen).
///  - CycleDetected: nur bei beschädigten Graphen; Graph::connect verhindert Zyklen.
///
/// Werte von außen:
///  - Modulation: Ein Eingang mit dem Schlüssel eines Parameters ändert den Shader
///    nicht; seine Quelle wirkt erst in packParameters (Parameter-Slot).
///  - Scalar oder Farbe an einem Field-Eingang wird ein Konstanten-Slot
///    (SlotOrigin::FieldConstant) und im Shader ein konstantes Feld. Die Quelle
///    wird nicht übersetzt; ihren Wert liefert packParameters.
[[nodiscard]] Result<CompiledField> compileField(const graph::Graph& graph,
                                                 const ISnippetLibrary& snippets,
                                                 const graph::PortRef& target);

}  // namespace whitehole::compiler
