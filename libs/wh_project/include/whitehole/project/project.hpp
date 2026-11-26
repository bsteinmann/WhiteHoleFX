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
#include "whitehole/project/cue.hpp"
#include "whitehole/project/live_grid.hpp"

namespace whitehole::project {

inline constexpr int kFormatVersion = 1;

/// Ein gespeicherter Effekt ("Look"): die Nodes vom Generator bis zum Ausgang als eigener kleiner Graph,
/// dazu der Field-Ausgang und die Zeit, in der er beim Live-Betrieb eingeblendet wird. Looks gehören zur
/// Projektdatei (Abschnitt `look ... endlook`), damit eine Show in einer Datei bleibt.
/// Winziges Standbild eines Looks (RGB, 3 Bytes je Pixel, Zeile für Zeile), das beim Speichern aufgenommen wird und
/// im Live-Fenster auf dem Look-Knopf erscheint. Nur Anzeige, ohne Einfluss auf die Ausgabe.
struct LookThumbnail {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgb;  // width * height * 3 Bytes
};

inline constexpr std::uint32_t kMaxThumbnailSide = 128;

struct Look {
    explicit Look(std::shared_ptr<const graph::INodeRegistry> registry) : graph{std::move(registry)} {}

    std::string name;
    double fadeSeconds{2.0};  // Überblendzeit beim Abrufen, 0 = hartes Umschalten
    LookTrigger trigger{LookTrigger::Toggle};
    /// Nur Flash: Zeit für das Einblenden beim Drücken. Leer = die Überblendzeit dieses Looks (bzw. die eigene Zeit des Live-Fensters).
    std::optional<double> flashInSeconds;
    /// Nur Flash: Zeit für das Zurückblenden beim Loslassen. Leer = die Überblendzeit dieses Looks.
    std::optional<double> returnSeconds;
    /// Standbild für den Look-Knopf; leer, wenn der Look noch keines hat (ältere Projekte).
    std::optional<LookThumbnail> thumbnail;
    graph::Graph graph;
    graph::PortRef target;    // Field-Ausgang des Looks
};

inline constexpr double kMaxFadeSeconds = 600.0;

/// Eine benannte Gruppe von Fixtures (Namen), z. B. "Bühne links". Ziel für Output-Nodes.
struct FixtureGroup {
    std::string name;
    std::vector<std::string> fixtures;  // Namen der Fixtures im Layout
};

/// Ein benanntes Rechteck der Bühne (Anteile 0..1 der Leinwand, `top` zählt von oben wie beim Output-Node).
struct StageRegion {
    std::string name;
    float left{0.0F};
    float top{0.0F};
    float width{1.0F};
    float height{1.0F};
};

/// Ein Eintrag der Fixture-Bibliothek des Projekts: eine benannte Bauform (Matrix, Strip, Ring, Säule ...). Fixtures im
/// Layout können sie über Fixture::type verwenden; ändert man die Bauform, folgen alle Exemplare.
struct FixtureType {
    std::string name;
    mapping::FixtureShape shape;
};

struct Project {
    explicit Project(std::shared_ptr<const graph::INodeRegistry> registry)
        : graph{std::move(registry)} {}

    mapping::Layout layout;
    /// Bibliothek der Bauformen (Datei: `fixturetype`-Blöcke vor den Fixtures). Namen sind eindeutig.
    std::vector<FixtureType> types;
    mapping::NormalizeMode normalize{mapping::NormalizeMode::Stretch};
    /// Gruppen und Regionen als Ziele für Output-Nodes (NodeInstance::targets). Namen sind je Liste eindeutig.
    std::vector<FixtureGroup> groups;
    std::vector<StageRegion> regions;
    std::vector<output::FixturePatch> patches;
    /// Patches des internen DMX-Kanal-Pools auf echte Controller/Universe/Kanal (Datei: `dmxpatch`-Zeilen), siehe
    /// output_plan.hpp. Nicht an ein Fixture gebunden, darum eine eigene Liste statt in `patches`.
    std::vector<output::DmxChannelPatch> dmxPatches;

    graph::Graph graph;
    /// Field-Ausgang, der auf die LEDs abgetastet wird.
    std::optional<graph::PortRef> target;

    /// Die Look-Liste für den Live-Betrieb, in der Reihenfolge der Datei. Namen sind eindeutig.
    std::vector<Look> looks;

    /// Das feste Live-Raster mit seinen belegten Feldern (Datei: `livegrid` und `cell`-Zeilen). `columns == 0` heißt:
    /// die Datei hat keines; der Player baut es dann aus den Looks (makeLiveGridFromLooks).
    LiveGrid liveGrid;

    /// Die Cuelist (Issue #21): Looks in fester Reihenfolge mit Auslösern und Zeiten. Leer = keine Cuelist.
    std::vector<Cue> cues;

    std::vector<output::Destination> destinations;
    std::string sourceName{"WhiteHoleFX"};
    std::uint8_t sacnPriority{100};
    double framesPerSecond{40.0};
    /// Issue #25: Aus (Standard) gilt ein unverbundener Eingang von Blend, Mix, Crossfade, Add und Mask als durchsichtiges
    /// Schwarz. An (Datei: `settings ... strict-inputs on`) ist er wie früher ein Pflicht-Eingang. Die Graphen (Haupt
    /// und Looks) tragen dasselbe als Graph::strictInputs; applyStrictInputs() hält beides gleich.
    bool strictInputs{false};
};

/// Übernahme alter Projekte: ein Raster mit `columns` Spalten (mindestens `rows` Zeilen, bei mehr Looks entsprechend
/// mehr), die Looks zeilenweise darin, mit Trigger und Zeiten des Looks. Leer, wenn `columns` nicht in 1..16 liegt.
[[nodiscard]] LiveGrid makeLiveGridFromLooks(const std::vector<Look>& looks, int columns, int rows);

/// Setzt strictInputs im Projekt und in allen Graphen (Hauptgraph und Looks).
void applyStrictInputs(Project& project, bool strict);

/// Fehler: InvalidArgument mit "Zeile N: ..." in der Meldung; NotFound für unbekannte
/// Node-Typen, Parameter und Ports; sonst die Fehler von Layout und Graph.
[[nodiscard]] Result<Project> parseProject(std::string_view text,
                                           std::shared_ptr<const graph::INodeRegistry> registry);

/// Fehler: InvalidArgument, wenn das Projekt etwas enthält, das das Format nicht ausdrücken
/// kann (z. B. Anführungszeichen oder Zeilenumbruch in Namen werden maskiert, aber ein
/// Nodename ohne Zeichen nicht).
[[nodiscard]] Result<std::string> writeProject(const Project& project);

/// Die `destination`-Zeilen der Controller als Text (ohne Kopfzeile), so wie `writeProject` sie schreibt.
[[nodiscard]] std::string writeDestinations(const std::vector<output::Destination>& destinations);

/// Benutzerbibliothek der Controller (Name, Protokoll, Adresse, Port): eine Datei mit `destination`-Zeilen, jede mit
/// `name`. Fehler: Controller ohne Namen oder mit doppeltem Namen, sonst wie parseProject.
[[nodiscard]] std::string writeControllerLibrary(const std::vector<output::Destination>& controllers);
[[nodiscard]] Result<std::vector<output::Destination>> parseControllerLibrary(
    std::string_view text, std::shared_ptr<const graph::INodeRegistry> registry);

/// Stellt alle Patches auf das Protokoll ihres Controllers um (Art-Net <-> sACN verschiebt das Universe um 1, siehe
/// output/patch_protocol.hpp). Liefert die Zahl der umgestellten Patches. Bei einem Fehler bleibt `project` teilweise
/// verändert; der Aufrufer arbeitet auf einer Kopie (Player::editProject tut das).
[[nodiscard]] Result<int> alignPatches(Project& project);

/// Die `fixturetype`-Blöcke der Bauformen als Text (ohne Kopfzeile), so wie `writeProject` sie schreibt.
[[nodiscard]] std::string writeFixtureTypes(const std::vector<FixtureType>& types);

/// Export eines oder mehrerer Fixtures als eigene Datei (Reiter Fixtures → Export): Kopfzeile plus `fixturetype`-
/// Blöcke, dieselbe Schreibweise wie die Projektdatei.
[[nodiscard]] std::string writeFixtureExport(const std::vector<FixtureType>& types);
/// Liest eine solche Datei (Import). Fehler wie bei parseProject (zum Beispiel neuere Syntax, die dieses Programm
/// nicht kennt).
[[nodiscard]] Result<std::vector<FixtureType>> parseFixtureExport(std::string_view text,
                                                                  std::shared_ptr<const graph::INodeRegistry> registry);

/// Hilfen für den Runner.
[[nodiscard]] Result<std::string> readTextFile(const std::string& path);
/// Schreibt die Datei neu (ersetzt vorhandenen Inhalt). Fehler: IoError.
[[nodiscard]] Result<void> writeTextFile(const std::string& path, std::string_view text);

}  // namespace whitehole::project
