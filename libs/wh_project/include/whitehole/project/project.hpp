// project.hpp: ein ganzes Projekt als Wert und seine Textdatei.
//
// Warum es diese Datei gibt: Layout, Effektgraph und Ausgabe müssen zusammen gespeichert und
// wieder geladen werden, damit man ein Setup nicht jedes Mal neu zusammenbauen muss. Der
// Konsolen-Runner liest diese Datei; die spätere GUI schreibt sie.
//
// Format: Text, zeilenweise, mit Kommentaren (#). Es ist von Hand les- und schreibbar und lässt
// sich gut in Git vergleichen. Die genaue Sprache steht in docs/project-format.md.
//
// Aufgabe: `parseProject` baut aus Text ein Project und prüft dabei alles, was ohne GPU prüfbar
// ist (Formen, Parameter, Verbindungen, Patches). `writeProject` ist die Umkehrung; was
// geschrieben wird, liest `parseProject` zum selben Projekt zurück.
//
// Abhängigkeiten: wh_graph, wh_mapping, wh_output (nur Datentypen, kein Netzwerk).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/mapping/layout.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/output/output_plan.hpp"

namespace whitehole::project {

inline constexpr int kFormatVersion = 1;

struct Project {
    explicit Project(std::shared_ptr<const graph::INodeRegistry> registry)
        : graph{std::move(registry)} {}

    mapping::Layout layout;
    mapping::NormalizeMode normalize{mapping::NormalizeMode::Stretch};
    std::vector<output::FixturePatch> patches;

    graph::Graph graph;
    /// Field-Ausgang, der auf die LEDs abgetastet wird.
    std::optional<graph::PortRef> target;

    std::vector<output::Destination> destinations;
    std::string sourceName{"WhiteHoleFX"};
    std::uint8_t sacnPriority{100};
    double framesPerSecond{40.0};
};

/// Fehler: InvalidArgument mit "Zeile N: ..." in der Meldung; NotFound für unbekannte
/// Node-Typen, Parameter und Ports; sonst die Fehler von Layout und Graph.
[[nodiscard]] Result<Project> parseProject(std::string_view text,
                                           std::shared_ptr<const graph::INodeRegistry> registry);

/// Fehler: InvalidArgument, wenn das Projekt etwas enthält, das das Format nicht ausdrücken
/// kann (z. B. Anführungszeichen oder Zeilenumbruch in Namen werden maskiert, aber ein
/// Nodename ohne Zeichen nicht).
[[nodiscard]] Result<std::string> writeProject(const Project& project);

/// Hilfen für den Runner.
[[nodiscard]] Result<std::string> readTextFile(const std::string& path);

}  // namespace whitehole::project
