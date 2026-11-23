// player.hpp: das, was eine Oberfläche mit einem Projekt tut, ohne etwas von Qt zu wissen.
//
// Warum es diese Datei gibt: Laden, Starten, Stoppen, einen Frame weiterschalten und das
// Vorschaubild holen sind die Handgriffe jeder Oberfläche. Stünden sie im Fenstercode, ließen sie
// sich nur mit einem Fenster prüfen. Hier sind sie ein gewöhnliches Objekt, das ohne GPU und ohne
// Bildschirm getestet wird; die Qt-Schicht ruft nur noch auf und zeichnet.
//
// Zustände:
//   leer -> (load) geladen -> (start) läuft -> (stop) geladen.
//   Ein fehlgeschlagenes load lässt das bisherige Projekt unverändert. load während des Laufs
//   hält erst an.
//
// Ohne GPU-Gerät (nullptr) funktioniert alles bis auf start(); die Vorschau zeigt dann das Layout.
//
// Threading: alles auf einem Thread (dem der Oberfläche). Die Ausgabe läuft in der Session in
// eigenem Thread, das ist von außen nicht sichtbar.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/gpu/gpu_device.hpp"
#include "whitehole/mapping/region_edit.hpp"
#include "whitehole/graph/node_docking.hpp"
#include "whitehole/graph/node_placement.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/packet_sink.hpp"
#include "whitehole/output/sent_dmx_store.hpp"
#include "whitehole/preview/preview.hpp"
#include "whitehole/preview/view3d.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/audio_usage.hpp"
#include "whitehole/runtime/dmx_listener.hpp"
#include "whitehole/runtime/dmx_output_usage.hpp"
#include "whitehole/runtime/dmx_usage.hpp"
#include "whitehole/runtime/led_info.hpp"
#include "whitehole/runtime/live_signals.hpp"
#include "whitehole/runtime/cue_runner.hpp"
#include "whitehole/runtime/live_decks.hpp"
#include "whitehole/runtime/live_dmx.hpp"
#include "whitehole/runtime/master_dmx.hpp"
#include "whitehole/runtime/node_clipboard.hpp"
#include "whitehole/runtime/node_defaults.hpp"
#include "whitehole/runtime/session.hpp"

namespace whitehole::runtime {

struct PlayerStatus {
    bool loaded{false};
    bool running{false};
    std::uint64_t frames{0};
    std::uint64_t framesDropped{0};
    std::uint64_t packets{0};
    std::uint64_t sendErrors{0};
    std::uint32_t ledCount{0};
    double framesPerSecond{40.0};
    std::string path;                 // zuletzt geladene Datei (leer bei loadText)
    std::optional<Error> lastError;   // letzter Fehler von Ausgabe oder Effekt
};

/// Ein einstellbarer Parameter eines Nodes, wie ihn eine Oberfläche braucht.
struct ParameterInfo {
    std::string key;
    std::string displayName;
    graph::ParameterValue value;
    graph::ParameterValue defaultValue;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    /// Namen der Auswahlmöglichkeiten (nur bei Ganzzahlen, Wert = Index); leer = gewöhnliche Zahl.
    std::vector<std::string> choices;
    /// Siehe graph::ParameterDescriptor::visibleWhenAtLeastKey/-AtLeast: steuert nur die Oberfläche.
    std::string visibleWhenAtLeastKey;
    std::optional<double> visibleWhenAtLeast;
    /// Ein Eingang mit demselben Schlüssel ist verbunden: Der Wert kommt aus dem Graphen, ein
    /// Regler hätte keine Wirkung.
    bool driven{false};
    /// Bei `driven`: die Verbindung samt Skalierung am Kabel (graph::Connection::scale, offset).
    ConnectionId connection{};
    double scale{1.0};
    double offset{0.0};
};

struct NodeInfo {
    NodeId id;
    std::string title;  // Anzeigename des Typs, bei mehreren gleichen mit Nummer ("Linear Gradient 2")
    std::string typeId;
    std::vector<ParameterInfo> parameters;
};

class Player {
public:
    /// `device` darf nullptr sein. Gerät, Bibliothek und Sink müssen den Player überleben.
    Player(gpu::IGpuDevice* device, const nodes::BuiltinLibrary& library, output::IPacketSink& sink)
        : device_{device}, library_{library}, sink_{sink}, outputMonitor_{std::make_shared<output::SentDmxStore>()} {}
    ~Player() { stop(); }
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    /// Liest und prüft die Datei samt Ausgabeplan. Fehler: die von readTextFile, parseProject
    /// und buildOutput. Ein Fehler lässt Zustand und Projekt unverändert.
    Result<void> load(const std::string& path);
    Result<void> loadText(std::string_view text);

    /// Fehler: InvalidArgument (nichts geladen, kein Gerät), sonst die von Session::create.
    Result<void> start();
    void stop();

    /// Ein Frame. Ohne Laufen ein No-op. Fehler beim Einreichen stoppen den Lauf.
    Result<void> step();

    [[nodiscard]] PlayerStatus status() const;
    [[nodiscard]] const project::Project* project() const noexcept { return project_ ? &*project_ : nullptr; }
    /// Bildrate des Projekts (40, wenn keins geladen ist).
    [[nodiscard]] double framesPerSecond() const noexcept { return project_ ? project_->framesPerSecond : 40.0; }

    /// Die Node-Typen, mit denen dieser Player arbeitet (für Presets).
    [[nodiscard]] std::shared_ptr<const graph::INodeRegistry> registry() const { return library_.registry; }

    /// Für den DMX-Ausgangs-Monitor (Studio): zuletzt gesendete Art-Net/sACN-Universen, überlebt den
    /// Wechsel von Projekten/Sessions (anders als die Pipeline selbst). Leer, solange nicht gesendet wird.
    [[nodiscard]] std::shared_ptr<output::SentDmxStore> outputMonitor() const noexcept { return outputMonitor_; }

    /// Alle Nodes mit mindestens einem Parameter, in Graph-Reihenfolge. Leer ohne Projekt.
    [[nodiscard]] std::vector<NodeInfo> nodes() const;

    /// Ändert einen Parameter. Läuft das Projekt, wirkt es ab dem nächsten step(); mehrere
    /// Änderungen zwischen zwei Frames werden zu einer Übernahme zusammengefasst. Liefert den
    /// tatsächlich gespeicherten Wert (Zahlen werden auf den erlaubten Bereich begrenzt).
    /// Fehler: InvalidArgument (kein Projekt, falscher Typ), NotFound (Node oder Parameter).
    Result<graph::ParameterValue> setParameter(NodeId node, const std::string& key, graph::ParameterValue value);

    // --- Graph bearbeiten (Node-Editor) ---------------------------------------------------------
    // Alle Änderungen wirken im laufenden Betrieb ab dem nächsten step(). Ist der Graph
    // zwischendurch unvollständig, läuft der letzte gute Zustand weiter und status().lastError
    // nennt den Grund. Fehler (alle): InvalidArgument, wenn kein Projekt geladen ist.

    /// Legt einen Node mit Standardwerten an. Fehler: NotFound (Typ unbekannt).
    Result<NodeId> addNode(std::string_view typeId, Vec2 position);

    /// Legt einen Node an und verdrahtet ihn nach `plan` (siehe graph::planDocking: einfügen in ein Kabel oder an einen
    /// Node hängen). Ein einziger Schritt für Rückgängig; scheitert ein Kabel, ändert sich nichts. Nicht für den
    /// Ausgangs-Node (wh.output.canvas), der sich beim Anlegen selbst verdrahtet. `moves` verschiebt dabei andere
    /// Nodes, die Platz machen (graph::placeDockedNode), im selben Schritt. Fehler: InvalidArgument, NotFound
    /// (Typ unbekannt) und die von Graph::connect.
    Result<NodeId> addNodeDocked(std::string_view typeId, Vec2 position, const graph::DockPlan& plan,
                                 const std::vector<graph::NodeMove>& moves = {});

    /// Eigene Standardwerte für neue Nodes (Einstellungen). Sie gelten nur für künftig angelegte Nodes; ein
    /// Wert, der nicht zum Parameter passt, wird beim Anlegen übersprungen. Gehören dem Nutzer, nicht dem Projekt.
    void setNodeDefaults(NodeDefaults defaults) { nodeDefaults_ = std::move(defaults); }
    [[nodiscard]] const NodeDefaults& nodeDefaults() const noexcept { return nodeDefaults_; }
    /// Entfernt Node und Verbindungen. Liefert der Node den Ausgang (`output`), rückt der Ausgang
    /// auf den Node davor (erster belegter Field-Eingang). Gibt es keinen, hat das Projekt danach
    /// keinen Ausgang und start() meldet InvalidArgument, bis man mit setTarget einen setzt.
    Result<void> removeNode(NodeId node);
    /// Fügt kopierte Nodes samt Parametern und Kabeln ein, `offset` wird auf ihre Positionen
    /// addiert. Ein Schritt für Rückgängig; scheitert etwas, ändert sich nichts. Liefert die neuen
    /// Ids in der Reihenfolge der Zwischenablage. Fehler: InvalidArgument (leer, kein Projekt) und
    /// die von Graph::addNode, setParameter und connect.
    Result<std::vector<NodeId>> pasteNodes(const NodeClipboard& clipboard, Vec2 offset);
    /// Verbindet zwei Ports. Ist der Eingang belegt, wird die alte Verbindung ersetzt; scheitert
    /// die neue, bleibt die alte erhalten. Fehler wie Graph::connect (ohne AlreadyExists).
    Result<ConnectionId> connect(const graph::PortRef& from, const graph::PortRef& to);
    Result<void> disconnect(ConnectionId connection);
    /// Skalierung am Kabel (siehe graph::Connection::scale): Faktor und Offset einer Verbindung. Mehrere Änderungen
    /// derselben Verbindung hintereinander (Regler ziehen) sind ein Schritt Rückgängig.
    Result<void> setConnectionScale(ConnectionId connection, double scale, double offset);
    /// Lage im Editor; ändert den Effekt nicht.
    Result<void> moveNode(NodeId node, Vec2 position);
    /// Eigener Name des Nodes (siehe graph::NodeInstance::label); ändert den Effekt nicht. Leer löscht den
    /// eigenen Namen wieder. Fehler: Graph::setLabel (NotFound, InvalidArgument bei zu langem Namen).
    Result<void> renameNode(NodeId node, std::string label);

    /// Gruppen (siehe graph/node_group.hpp): reine Oberfläche, der Effekt ändert sich nicht. Alle Schritte sind
    /// Rückgängig-fähig. Fehler wie Graph::addGroup / replaceGroup / removeGroup.
    Result<void> addNodeGroup(graph::NodeGroup group);
    Result<void> replaceNodeGroup(const std::string& name, graph::NodeGroup group);
    /// Löst die Gruppe auf; die Nodes bleiben.
    Result<void> removeNodeGroup(const std::string& name);
    Result<void> setNodeGroupCollapsed(const std::string& name, bool collapsed);
    /// Lage der Kachel; mehrere Schritte hintereinander (Ziehen) sind ein Schritt Rückgängig.
    Result<void> moveNodeGroup(const std::string& name, Vec2 position);

    /// Überbrückt einen Field-Node oder schaltet ihn wieder ein (Undo-fähig, baut den Shader neu).
    /// Fehler: Unsupported für Nodes ohne Field-Domäne.
    Result<void> setNodeBypassed(NodeId node, bool bypassed);
    /// Wie setNodeBypassed für mehrere Nodes in einem Rückgängig-Schritt. Scheitert einer, ändert sich nichts.
    Result<void> setNodesBypassed(const std::vector<NodeId>& nodes, bool bypassed);
    /// Wählt den Field-Ausgang, der auf die LEDs abgetastet wird. Läuft das Projekt, wird es im
    /// nächsten step() neu aufgebaut (kurze Unterbrechung der Ausgabe).
    /// Fehler: NotFound (Node oder Port), IncompatiblePorts (kein Field-Ausgang).
    /// Mehrere Outputs: Ziel eines Output-Nodes (Einträge "fixture:<Name>", "group:<Name>", "region:<Name>").
    /// Das Rechteck des Nodes wird aus dem Ziel berechnet und folgt späteren Layout-Änderungen. Leer = freies Rechteck.
    /// Fehler: NotFound, Unsupported (kein Output-Node). Ein Undo-Schritt.
    Result<void> setOutputTargets(NodeId node, std::vector<std::string> targets);
    /// Ersetzt alle Gruppen bzw. Regionen (Namen eindeutig). Fehler: InvalidArgument. Ein Undo-Schritt.
    Result<void> setGroups(std::vector<project::FixtureGroup> groups);
    Result<void> setRegions(std::vector<project::StageRegion> regions);

    Result<void> setTarget(const graph::PortRef& port);

    // --- Rückgängig / Wiederholen -----------------------------------------------------------------
    // Jede Änderung am Projekt (Parameter, Graph, Lage) merkt sich vorher den alten Stand. Viele
    // Änderungen desselben Reglers oder desselben Nodes hintereinander (Ziehen) zählen als eine.
    // Laden löscht beide Listen, Speichern nicht.
    [[nodiscard]] bool canUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_.empty(); }
    /// Fehler: InvalidArgument, wenn nichts zurückzunehmen ist.
    Result<void> undo();
    /// Beendet das Zusammenfassen gleichartiger Änderungen: die nächste Änderung bekommt einen
    /// eigenen Undo-Schritt. Die Oberfläche ruft das beim Loslassen der Maus auf, damit zwei
    /// getrennte Ziehbewegungen nicht zu einem Schritt verschmelzen.
    void closeEditGroup() noexcept { lastEditKey_.clear(); }
    Result<void> redo();

    // --- Layout, Ausgabe, Einstellungen bearbeiten (Layout-Editor) ---------------------------------
    // Jede Änderung wird samt Ausgabeplan geprüft (Universen, Kanäle, Ziele); scheitert die
    // Prüfung, bleibt alles wie es war und der Fehler nennt den Grund. Eine laufende Ausgabe wird
    // im nächsten step() neu aufgebaut (kurze Unterbrechung). Fehler (alle): InvalidArgument ohne
    // Projekt; Fehler des Layouts (Form ungültig, Name doppelt oder leer) und des Ausgabeplans.

    /// Legt ein Fixture an, optional mit Patch (der Eintrag `fixture` wird ersetzt).
    Result<mapping::FixtureId> addFixture(std::string name, const Vec3& origin, mapping::FixtureShape shape,
                                          std::optional<output::FixturePatch> patch = std::nullopt);
    /// Entfernt das Fixture samt Patch. Das letzte gepatchte Fixture bleibt (sonst gäbe es keine Ausgabe).
    Result<void> removeFixture(mapping::FixtureId id);
    Result<void> renameFixture(mapping::FixtureId id, std::string name);
    Result<void> moveFixture(mapping::FixtureId id, const Vec3& origin);
    Result<void> setFixtureShape(mapping::FixtureId id, mapping::FixtureShape shape);

    // Fixture-Bibliothek (Bauformen) und Drehung. Die Exemplare im Layout verweisen über Fixture::type auf eine
    // Bauform; ändert man diese, folgen alle Exemplare. Alles ist ein Schritt Rückgängig.
    [[nodiscard]] const std::vector<project::FixtureType>& fixtureTypes() const;
    /// Neue Bauform. Fehler: InvalidArgument (Name, Form), AlreadyExists.
    Result<void> addFixtureType(std::string name, mapping::FixtureShape shape);
    /// Tauscht die Form der Bauform und aller Exemplare. Fehler: NotFound, InvalidArgument.
    Result<void> replaceFixtureType(const std::string& name, mapping::FixtureShape shape);
    Result<void> renameFixtureType(const std::string& from, std::string to);
    /// Entfernt die Bauform; die Exemplare behalten ihre Form als eigene. Fehler: NotFound.
    Result<void> removeFixtureType(const std::string& name);
    /// Für ein frisch angelegtes Projekt (Vorlage "Matrix 16x8"): Hat es genau ein Fixture und gehört dessen Bauform sonst
    /// niemandem, verschwindet die Bauform aus der Bibliothek; das Fixture behält seine Form. So steht in der
    /// Bibliothek nur, was der Nutzer selbst anlegt. Ohne passenden Fall (kein Projekt, mehrere Fixtures, kein Typ)
    /// geschieht nichts und es ist kein Fehler.
    Result<void> dropSoleFixtureType();
    /// Setzt die Bauform eines Fixtures (übernimmt deren Form); mit leerem Namen wird es von der Bibliothek gelöst.
    Result<void> assignFixtureType(mapping::FixtureId id, const std::string& typeName);
    /// Legt ein Exemplar einer Bauform an (Lage, Drehung, optional Patch).
    Result<mapping::FixtureId> addFixtureOfType(std::string name, const std::string& typeName, const Vec3& origin,
                                                const Vec3& rotation, std::optional<output::FixturePatch> patch = std::nullopt);
    /// Drehung in Grad um x, y, z (um die Mitte des Fixtures).
    Result<void> setFixtureRotation(mapping::FixtureId id, const Vec3& degrees);
    /// Verschiebt mehrere Fixtures um denselben Versatz (ein Undo-Schritt, wie ein Ziehen mit der Maus).
    /// Fehler: NotFound bei unbekannter Id, InvalidArgument bei nicht endlichem Versatz.
    Result<void> moveFixtures(const std::vector<mapping::FixtureId>& ids, const Vec3& delta);

    /// Ausrichten an der Gesamtbox der gewählten Fixtures. Oben/unten und links/rechts sind die
    /// Richtungen der Vorschau (y nach oben, x nach rechts).
    enum class Align : std::uint8_t { Left, HCenter, Right, Top, VCenter, Bottom };
    /// Achse zum Aneinanderreihen.
    enum class Axis : std::uint8_t { X, Y, Z };
    /// Dreht mehrere Fixtures um dieselbe feste Weltachse, jedes um die eigene Mitte (ein Undo-Schritt, aufeinander
    /// folgende Drehungen derselben Fixtures verschmelzen wie beim Ziehen). Die Winkel x, y, z der Fixtures dürfen
    /// sich dabei ändern (siehe mapping::rotationAfterWorldTurn). Fehler: NotFound (unbekannte Id), InvalidArgument
    /// (Liste leer, Winkel nicht endlich).
    Result<void> rotateFixtures(const std::vector<mapping::FixtureId>& ids, Axis axis, float degrees);
    /// Richtet die Fixtures aus (ein Undo-Schritt). Fehler: NotFound (unbekannte Id), InvalidArgument
    /// (Liste leer).
    Result<void> alignFixtures(const std::vector<mapping::FixtureId>& ids, Align mode);
    /// Reiht die Fixtures auf der Achse nebeneinander, in der Reihenfolge ihrer Lage, mit `gap` Abstand
    /// zwischen den Boxen; das erste bleibt stehen, die anderen rücken an (ein Undo-Schritt). Fehler:
    /// InvalidArgument bei weniger als zwei Fixtures oder einem Abstand unter 0, NotFound bei unbekannter Id.
    Result<void> packFixtures(const std::vector<mapping::FixtureId>& ids, Axis axis, float gap = 0.0F);
    /// Stellt die Projektion mehrerer Fixtures um (Volume/Faces/Box, siehe mapping::Projection). Ein Undo-Schritt.
    /// Fehler: InvalidArgument bei leerer Auswahl, NotFound bei unbekannter Id.
    Result<void> setFixturesProjection(const std::vector<mapping::FixtureId>& ids, mapping::Projection projection);
    /// Stellt die Projektion aller Fixtures einer Gruppe um (ein Undo-Schritt). Die Gruppe nennt ihre Fixtures über
    /// den Namen. Fehler: NotFound bei unbekannter Gruppe oder einem Namen, der im Layout fehlt.
    Result<void> setGroupProjection(const std::string& group, mapping::Projection projection);

    /// Die Box der ganzen Wand (alle Fixtures) in Weltkoordinaten. Ohne Projekt eine leere Box.
    [[nodiscard]] mapping::Box wallBox() const noexcept { return map_.volume; }
    /// Die LED-Karte des geladenen Projekts (Positionen und Bereiche je Fixture), z. B. für die Verkabelungsanzeige.
    [[nodiscard]] const mapping::LedMap& ledMap() const noexcept { return map_; }

    /// Setzt das Rechteck eines Region-Nodes (wh.transform.region) oder Ausgangs-Nodes
    /// (wh.output.canvas): left, top, width, height in
    /// Anteilen der Leinwand, y von oben. Ein Undo-Schritt (aufeinanderfolgende Aufrufe für denselben
    /// Node werden zusammengefasst, bis closeEditGroup() kommt). Fehler: NotFound (kein solcher
    /// Node), InvalidArgument (falscher Node-Typ, Rechteck nicht endlich).
    Result<void> setRegionRect(NodeId node, const mapping::Region& rect);

    /// Setzt oder (mit nullopt) entfernt den Patch des Fixtures.
    Result<void> setPatch(mapping::FixtureId id, std::optional<output::FixturePatch> patch);
    /// Ersetzt die Zielliste. Ein Patch darf nur auf vorhandene Ziele zeigen.
    Result<void> setDestinations(std::vector<output::Destination> destinations);
    /// Entfernt das Ziel mit dieser Nummer (nicht nur das letzte). Fixtures auf höheren Zielen rücken mit ihrer Nummer
    /// nach. Wird das Ziel noch von einem Fixture benutzt, geschieht nichts und die Meldung nennt dieses Fixture.
    /// Fehler: NotFound (keine solche Nummer), InvalidArgument (Ziel in Benutzung).
    Result<void> removeDestination(std::size_t index);

    /// Ersetzt die Liste der dmxpatch-Einträge (interner Kanal-Pool der DMX-Output-Nodes auf echte Ziele
    /// abgebildet, siehe docs/output.md). Validiert wie beim Laden: Ziel muss existieren, kein DDP, Universe/Kanal/
    /// interner Kanal im gültigen Bereich, keine zwei Einträge mit überschneidendem Pool-Bereich
    /// (OutputPipeline::create, Fehler InvalidArgument/AlreadyExists); bei Ablehnung bleibt alles unverändert.
    Result<void> setDmxPatches(std::vector<output::DmxChannelPatch> patches);

    /// Ein Patch mit dem ersten freien Universe hinter allen Art-Net/sACN-Patches auf Ziel 0
    /// (Art-Net, Reihenfolge RGB). `leds` ist die Zahl der LEDs des neuen Fixtures; der Vorschlag
    /// geht davon aus, dass je Universe 170 LEDs passen. Ohne Projekt Universe 0.
    [[nodiscard]] output::FixturePatch suggestPatch(std::uint64_t leds) const;

    /// Passt den Wunschkanal von `wanted` für das Fixture `id` an: Liegt er schon bei einem anderen Fixture im selben
    /// Universe, kommt das Fixture direkt hinter dessen letzten Kanal (die Matrix und dahinter ein einzelner Pixel im
    /// selben Universe). Passt es nirgends, bleibt `wanted` unverändert und `setPatch` meldet den Grund.
    [[nodiscard]] output::FixturePatch fitPatch(mapping::FixtureId id, output::FixturePatch wanted) const;

    struct Settings {
        double framesPerSecond{40.0};
        mapping::NormalizeMode normalize{mapping::NormalizeMode::Stretch};
        std::uint8_t sacnPriority{100};
        std::string sourceName{"WhiteHoleFX"};
    };
    /// Fehler: InvalidArgument (fps nicht in (0, 1000], Priorität über 200, Name leer).
    Result<void> setSettings(const Settings& settings);

    /// Seit dem letzten Laden oder Speichern verändert (Parameter oder Graph).
    [[nodiscard]] bool modified() const noexcept { return modified_; }

    /// Schreibt das Projekt (Layout, Graph, Ausgabe samt aktueller Parameter) als Datei und merkt
    /// sich den Pfad. Kommentare einer von Hand geschriebenen Datei gehen dabei verloren.
    Result<void> save(const std::string& path);

    // --- Testbild ------------------------------------------------------------------------------------
    // Ersetzt die Farben des Effekts durch ein Testbild (Farbe, Auswahl, Laufbalken ...). Gehört nicht
    // zum Projekt: wird nicht gespeichert, nicht rückgängig gemacht und beim Laden eines anderen
    // Projekts verworfen. Es wirkt in Vorschau und Ausgabe; Pakete gehen nur raus, solange der Sink
    // sendet. Ohne laufende Session wird das Bild gemerkt und beim Start angewendet.

    /// Fehler: InvalidArgument (kein Projekt, Farbe nicht endlich). Art Off entspricht clearTestPattern().
    Result<void> setTestPattern(TestPattern pattern);
    void clearTestPattern();
    /// Das aktive Testbild oder nullptr.
    [[nodiscard]] const TestPattern* testPattern() const noexcept {
        return testPattern_.active() ? &testPattern_ : nullptr;
    }

    // --- Master und Blackout -------------------------------------------------------------------------
    // Pulte für den Betrieb, nicht Teil des Projekts: nicht gespeichert, nicht im Undo, bleiben beim
    // Laden eines anderen Projekts erhalten. Wirken in Vorschau und Ausgabe, auch auf Testbilder.

    /// Gesamthelligkeit 0..1 (außerhalb oder nicht endlich wird begrenzt bzw. auf 1 gesetzt). Liefert den Wert.
    float setMasterBrightness(float master);
    [[nodiscard]] float masterBrightness() const noexcept { return master_; }
    /// Master über einen DMX-Kanal (Issue #17): Solange Pakete kommen, setzt jeder step() den Master auf Wert/255.
    /// `std::nullopt` schaltet es ab (der Master bleibt, wo er ist). Gespeichert wird nichts.
    void setMasterDmx(std::optional<MasterDmxSource> source);
    [[nodiscard]] const std::optional<MasterDmxSource>& masterDmx() const noexcept { return masterDmx_; }
    /// Wahr, wenn der Master gerade vom DMX-Kanal kommt (eingestellt und das Signal ist frisch).
    [[nodiscard]] bool masterFromDmx() const noexcept { return masterDmxLive_; }
    /// Blackout: alle LEDs aus, die Pakete laufen weiter (Controller bleiben verbunden).
    void setBlackout(bool on);
    [[nodiscard]] bool blackout() const noexcept { return blackout_; }
    /// Wahr, wenn auf dem Weg zum Ausgang ein Pflicht-Eingang unverbunden ist (zum Beispiel hat der Output-Node seine
    /// Quelle verloren). Dann sind die LEDs aus, statt den letzten Look zu halten; die Pakete laufen weiter.
    [[nodiscard]] bool outputIncomplete() const noexcept { return incomplete_; }

    // --- Looks und Live-Betrieb ------------------------------------------------------------------------
    // Ein Look ist ein fertiger Effekt (vom Generator bis zum Ausgang). Die Liste gehört zum Projekt und
    // steht in der Datei. Im Live-Betrieb laufen zwei Decks mit einer Überblendung (siehe live_decks.hpp);
    // der Editor-Graph ruht dann, und die Ausgabe zeigt nur noch Looks. Fehler (alle): InvalidArgument ohne Projekt.

    [[nodiscard]] const std::vector<project::Look>& looks() const;
    /// Speichert den Effekt hinter dem Ausgang als Look. Ein gleichnamiger Look wird ersetzt (Position
    /// und Überblendzeit bleiben, wenn `fadeSeconds` fehlt). Liefert den Index. Fehler: InvalidArgument
    /// (Name, Zeit, kein Ausgang), sonst die der Graph-Operationen.
    Result<int> saveLook(const std::string& name, std::optional<double> fadeSeconds = std::nullopt);
    /// Fehler: NotFound (Index), InvalidArgument (Name ungültig oder schon vergeben, Zeit außerhalb 0..600).
    Result<void> removeLook(int index);
    Result<void> renameLook(int index, const std::string& name);
    Result<void> setLookFade(int index, double seconds);
    /// Strenger Modus (Issue #25): an = unverbundene Blend-Eingänge sind Pflicht (LEDs aus), aus (Standard) = durchsichtiges
    /// Schwarz. Ein Undo-Schritt; nicht im Live-Betrieb. Fehler: InvalidArgument (kein Projekt, Live aktiv).
    Result<void> setStrictInputs(bool strict);
    [[nodiscard]] bool strictInputs() const;
    /// Verschiebt einen Look in der Liste an die Stelle `to`.
    Result<void> moveLook(int from, int to);
    /// Ersetzt den Editor-Graphen durch den Look (ein Undo-Schritt), um ihn zu bearbeiten. Nicht im
    /// Live-Betrieb. Fehler: NotFound, InvalidArgument (Live aktiv).
    Result<void> loadLookIntoEditor(int index);

    /// Wechselt in den Live-Betrieb und zeigt den Look `first` sofort. Fehler: NotFound (Index),
    /// InvalidArgument (keine Looks). Eine laufende Ausgabe wird dafür einmal neu aufgebaut.
    Result<void> startLive(int first = 0);
    /// Zurück zum Editor-Graphen (wieder ein Neuaufbau). Ohne Live-Betrieb ein No-op.
    void stopLive();
    [[nodiscard]] bool liveActive() const noexcept { return live_.has_value(); }
    /// Lädt den Look ins verdeckte Deck (Vorschau-Look, "Next"). Fehler: NotFound, InvalidArgument (nicht live).
    Result<void> liveArm(int index);
    /// Startet die Überblendung zum vorgeladenen Look. Ohne Zeit gilt die Zeit des Looks.
    Result<void> liveGo(std::optional<double> fadeSeconds = std::nullopt);
    /// liveArm + liveGo in einem Schritt (ein Tastendruck für einen Look).
    Result<void> liveTrigger(int index, std::optional<double> fadeSeconds = std::nullopt);
    [[nodiscard]] LiveStatus liveStatus() const noexcept;
    /// Taste gedrückt bzw. losgelassen (Pad, Knopf, DMX-Schwelle; player_live_flash.cpp, docs/entwurf-midi.md Abschnitt 5).
    /// Ein Look mit `trigger flash` blendet beim Drücken ein und beim Loslassen zum vorherigen zurück (Vorrang vor
    /// "Mit GO"); ein umschaltender Look folgt `useGo` (vorladen statt überblenden), liveUp lässt ihn unberührt.
    /// Ein Umschalten (auch über liveTrigger/liveArm/liveGo) beendet ein laufendes Flash: dann bleibt der neue Look stehen.
    /// Stellt ein, wie ein Look ausgelöst wird (umschalten/flashen) und wie lange das Zurückblenden dauert (leer = die
    /// Überblendzeit des Looks). Geht auch im Live-Betrieb. Fehler: NotFound, InvalidArgument (keine Projekt, Zeit ungültig).
    Result<void> setLookTrigger(int index, project::LookTrigger trigger, std::optional<double> returnSeconds);
    Result<void> liveDown(int index, std::optional<double> fadeSeconds = std::nullopt, bool useGo = false);
    Result<void> liveUp(int index);
    /// Gehaltene Flash-Looks, zuletzt gedrückter hinten (für Anzeige und Tests).
    [[nodiscard]] const std::vector<int>& liveFlashHeld() const noexcept { return flashHeld_; }
    /// Manuelle Überblendung zum vorgeladenen Look (Fader 0..1, siehe LiveDecks::setManualProgress).
    /// Fehler: InvalidArgument (nicht live). Ohne vorgeladenen Look wirkungslos.
    Result<void> liveManualFade(double progress);
    /// Uhr für die Überblendung in Sekunden (Standard: Systemuhr). Nur für Tests austauschbar.
    void setLiveClock(std::function<double()> clock) { liveClock_ = std::move(clock); }

    /// Look-Auswahl im Live-Betrieb per DMX-Kanal (siehe live_dmx.hpp): Solange Pakete kommen, löst jeder step()
    /// bei einem Wechsel des berechneten Look-Index liveTrigger aus. `std::nullopt` schaltet es ab. Gehört wie
    /// der Master zum Auftritt, nicht zum Projekt: wird nicht gespeichert.
    void setLiveDmx(std::optional<LiveDmxSource> source);
    [[nodiscard]] const std::optional<LiveDmxSource>& liveDmx() const noexcept { return liveDmx_; }
    /// Wahr, wenn die Look-Auswahl gerade vom DMX-Kanal kommt (eingestellt und das Signal ist frisch).
    [[nodiscard]] bool liveFromDmx() const noexcept { return liveDmxLive_; }

    // --- Live-Anzeige --------------------------------------------------------------------------------------------
    /// Die Werte der CPU-Nodes im zuletzt gerechneten Frame (für Balken und leuchtende Kabel im Editor). Leer ohne
    /// laufende Session. Nur vom GUI-Thread aus rufen (derselbe, der step() ruft). Nur Nodes, die im Frame wirklich
    /// ausgewertet wurden, tauchen auf; ein Node ohne Verbraucher liefert nichts.
    [[nodiscard]] std::vector<LiveSignal> liveSignals() const;

    // --- Cuelist (Issue #21) ---------------------------------------------------------------------------------
    // Die Cues stehen im Projekt (Zeilen `cue ...`). Der Ablauf braucht den Live-Betrieb: cuesStart() startet ihn,
    // zeigt den ersten Cue sofort und lässt dann die Auslöser (GO, Zeit, Taste, DMX, Beat) die weiteren auslösen.

    [[nodiscard]] const std::vector<project::Cue>& cues() const;
    /// Ersetzt die Cuelist (hält einen laufenden Ablauf an). Fehler: InvalidArgument (kein Projekt, Zeit außerhalb
    /// 0..3600, Kanal oder Schwelle außerhalb des Bereichs, Taste fehlt), NotFound (Look gibt es nicht).
    Result<void> setCues(std::vector<project::Cue> cues);
    /// Startet Live-Betrieb und Ablauf. Fehler: InvalidArgument (keine Cues, keine Looks), NotFound.
    Result<void> cuesStart();
    void cuesStop();
    [[nodiscard]] bool cuesRunning() const noexcept { return cueRunner_.running(); }
    /// GO: löst den nächsten Cue aus (wirkt beim nächsten step()).
    void cueGo() { cueGo_ = true; }
    /// Eine Taste wurde gedrückt (Name wie im Cue, z. B. "F5"); wirkt beim nächsten step().
    void cueKey(std::string key) { cueKeys_.push_back(std::move(key)); }
    /// Springt zu einem Cue (Doppelklick in der Liste). Fehler: InvalidArgument (läuft nicht), NotFound (Nummer).
    Result<void> cueFire(int index);
    [[nodiscard]] CueRunnerStatus cueStatus() const { return cueRunner_.status(liveNow()); }
    /// Letzter Fehler beim Ausführen eines Cues (z. B. ein Look, der sich nicht aufbauen lässt); leer, wenn keiner.
    [[nodiscard]] const std::optional<Error>& cueError() const noexcept { return cueError_; }

    /// Die LED unter einem Bildpunkt der Vorschau (`frame` ist der Ausschnitt des gezeichneten Bildes)
    /// samt Fixture, Nummer am Kabel, Adresse und Farbe. Leer, wenn keine LED näher als `maxDistance`
    /// Pixel liegt oder nichts geladen ist.
    [[nodiscard]] std::optional<LedInfo> ledInfoAt(const preview::ViewFrame& frame, float pixelX, float pixelY,
                                                   float maxDistance = 10.0F) const;

    /// Welche DMX-Universen die DMX-Nodes des geladenen Projekts lesen (leer, wenn nichts geladen ist).
    [[nodiscard]] DmxUsage dmxUsage() const;
    /// Wie viele Audio-Nodes das geladene Projekt hat (0, wenn nichts geladen ist).
    [[nodiscard]] AudioUsage audioUsage() const;
    /// Für den Rechtsklick im DMX-Ausgangs-Monitor (Schritt 3 des DMX-Patch/Monitor-Gitters, siehe
    /// CLAUDE.md): was auf diesen gesendeten Kanal gepatcht ist, falls etwas. Nullopt ohne Projekt.
    [[nodiscard]] std::optional<DmxOutputOccupant> dmxOutputOccupantAt(output::Protocol protocol, std::uint32_t universe,
                                                                        std::uint32_t channel) const;
    /// Für das "Wer hört zu"-Overlay im DMX-Monitor, Eingang (Schritt 4): wer diesen empfangenen Kanal
    /// liest - ein Node im Graphen, oder Master-Helligkeit/Live-DMX (Auftritts-Einstellungen, siehe
    /// masterDmx()/liveDmx()). Nullopt: nichts im geladenen Projekt bzw. Auftritt hört hier zu.
    [[nodiscard]] std::optional<DmxInputListener> dmxListenerAt(input::DmxProtocol protocol, std::uint32_t universe,
                                                                 std::uint32_t channel) const;
    /// Für die Rechtsklick-Kategorie "Node" im DMX-Monitor, Eingang (nachträglich freigegebene Erweiterung):
    /// alle wh.value.dmx/wh.value.dmxcolor-Nodes des geladenen Projekts, zum Patchen ihrer Adresse von dort
    /// aus - sowohl im gerade im Editor offenen Graphen als auch in jedem gespeicherten Look (siehe
    /// DmxValueNodeRef::lookIndex, dmx_listener.hpp). Leer ohne Projekt oder ohne passenden Node.
    [[nodiscard]] std::vector<DmxValueNodeRef> dmxValueNodes() const;
    /// Setzt einen Parameter eines Nodes, der NICHT im gerade offenen Graphen steht, sondern in einem
    /// gespeicherten, aktuell nicht geladenen Look (lookIndex wie von dmxValueNodes() geliefert) - für
    /// "Node" auf einen solchen Node. setParameter() selbst setzt immer project_->graph voraus (das ist
    /// der Live-/Editier-Graph) und kann einen Node in einem anderen Look nicht erreichen; diese Methode
    /// schreibt direkt in project_->looks[lookIndex].graph, ohne graphDirty_/valuesDirty_ zu setzen (der
    /// Look läuft gerade nicht live), markiert das Projekt aber als geändert (modified_), damit die
    /// Änderung beim Speichern mitgenommen wird.
    [[nodiscard]] Result<graph::ParameterValue> setLookNodeParameter(std::size_t lookIndex, NodeId node,
                                                                       const std::string& key, graph::ParameterValue value);
    /// Für dieselbe Rechtsklick-Kategorie "Node", aber im DMX-Ausgangs-Monitor (nachträglich freigegebene
    /// Erweiterung, 15.11.): alle wh.output.dmx/wh.output.dmxcolor-Nodes des geladenen Projekts, mit ihrem
    /// aktuellen internen Pool-Kanal (DmxOutputValueNodeRef::poolChannel/count) - ebenfalls Look-aware, wie
    /// dmxValueNodes(). Leer ohne Projekt oder ohne passenden Node.
    [[nodiscard]] std::vector<DmxOutputValueNodeRef> dmxOutputValueNodes() const;
    /// Patcht den Pool-Kanal-Bereich [poolChannel, poolChannel+count) (wie von dmxOutputValueNodes()
    /// geliefert) auf die angeklickte echte Adresse: legt in project_->dmxPatches einen neuen Eintrag an
    /// oder aktualisiert den bestehenden für genau diesen Pool-Bereich (eine bestehende destination bleibt
    /// dabei erhalten, ein neuer Eintrag bekommt 0 - derselbe Standard wie beim manuellen Patchen über Stage
    /// Config/Ausgabe). Anders als bei den Lese-Nodes gibt es hier keinen lookIndex-Parameter: die Adresse
    /// steht projektweit in project_->dmxPatches, nicht am Node selbst, der Look spielt also keine Rolle für
    /// das Schreiben (nur dmxOutputValueNodes() braucht ihn, um poolChannel/count aus dem richtigen Graphen
    /// zu lesen). Validierung (Überschneidungen, Bereichsgrenzen) wie setDmxPatches().
    [[nodiscard]] Result<void> patchDmxOutputValueNode(std::uint16_t poolChannel, std::uint16_t count, output::Protocol protocol,
                                                        std::uint32_t universe, std::uint32_t channel);

    /// Bild der LEDs: während des Laufs mit den Farben des letzten Frames (schwarz, solange noch
    /// keiner da ist), sonst das Layout. Fehler: InvalidArgument, wenn nichts geladen ist.
    [[nodiscard]] Result<preview::Image> image(const preview::PreviewOptions& options) const;

    /// Dasselbe durch eine frei drehbare Kamera (3D-Vorschau, nur zum Ansehen). Fehler wie `image`.
    [[nodiscard]] Result<preview::Image> image3d(const preview::Camera3d& camera,
                                                 const preview::PreviewOptions& options) const;

    /// Bühnenansicht für das Stage Config: die LEDs in Layout-Farben (eigener Farbton je Fixture, heller entlang des
    /// Kabels, erste LED weiß), unabhängig davon, ob gerade ein Effekt läuft. Mit `selected` bleiben nur diese
    /// Fixtures kräftig. Fehler wie `image3d`.
    [[nodiscard]] Result<preview::Image> stageImage3d(const preview::Camera3d& camera, const preview::PreviewOptions& options,
                                                      std::span<const mapping::FixtureId> selected) const;

private:
    /// Gamma je Fixture und aktuelle Farben für die Vorschau (2D und 3D).
    void previewInputs(preview::PreviewOptions& options, std::vector<ColorRgba>& colors) const;

    Result<void> adopt(project::Project project, std::string path);

    /// Vor jeder Änderung: liefert den Stand zum Merken (leer, wenn `key` die vorige Änderung
    /// fortsetzt). Nach der Änderung: finishEdit mit dem Ergebnis.
    [[nodiscard]] std::optional<project::Project> beginEdit(const std::string& key);
    void finishEdit(std::optional<project::Project> before, const std::string& key, bool ok, bool rebuild = false);
    /// Nach undo/redo: Session und Ausgabe an den neuen Stand anpassen.
    Result<void> restored(const std::optional<graph::PortRef>& previousTarget, bool rebuild);
    /// Führt `edit` auf dem Projekt aus und prüft danach den Ausgabeplan; scheitert eines von
    /// beiden, bleibt das Projekt unverändert. Erfolg: Undo-Schritt, Vorschau-Map neu, Session neu.
    Result<void> editProject(const std::string& key, const std::function<Result<void>(project::Project&)>& edit);

    /// Der Graph, den die Session gerade rechnet: im Live-Betrieb der der Decks, sonst der des Projekts.
    [[nodiscard]] const graph::Graph& activeGraph() const noexcept { return live_ ? live_->graph() : project_->graph; }
    Result<void> startLiveLook(int index);
    /// Prüft den Graphen des Ausgangs auf unverbundene Pflicht-Eingänge und gibt den Pegel an die Session weiter.
    [[nodiscard]] bool targetIncomplete() const;
    void refreshCompleteness();
    void applyOutputLevel();
    void pollMasterDmx();
    void pollLiveDmx();
    void advanceCues();
    Result<void> executeCue(int index);
    [[nodiscard]] double liveNow() const {
        return liveClock_ ? liveClock_()
                          : std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NodeDefaults nodeDefaults_;
    gpu::IGpuDevice* device_;
    const nodes::BuiltinLibrary& library_;
    output::IPacketSink& sink_;
    std::shared_ptr<output::SentDmxStore> outputMonitor_;  // überlebt Session-Neubauten, siehe outputMonitor()

    std::optional<project::Project> project_;
    std::string path_;
    mapping::LedMap map_;  // des geladenen Projekts, für die Layout-Vorschau
    std::unique_ptr<engine::SteadyFrameClock> clock_;
    std::unique_ptr<Session> session_;
    std::optional<Error> stepError_;
    bool graphDirty_{false};   // Parameter oder Graph geändert, Session noch nicht informiert
    bool valuesDirty_{false};  // nur Parameterwerte geändert: der billige Abgleich genügt
    std::optional<LiveDecks> live_;
    std::function<double()> liveClock_;
    std::optional<double> liveLast_;  // Zeitpunkt des letzten Schritts, solange ein Fade läuft
    bool rebuildDirty_{false};  // Ausgang, Layout oder Ausgabe geändert: Session neu aufbauen
    bool modified_{false};     // seit Laden/Speichern verändert
    struct Snapshot {
        project::Project project;
        bool rebuild{false};  // die Änderung zwischen diesem und dem nächsten Stand brauchte eine neue Session
    };
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    std::string lastEditKey_;  // zum Zusammenfassen gleichartiger Änderungen
    TestPattern testPattern_;  // Art Off = kein Testbild
    float master_{1.0F};
    std::optional<MasterDmxSource> masterDmx_;
    bool masterDmxLive_{false};
    std::optional<LiveDmxSource> liveDmx_;
    bool liveDmxLive_{false};
    std::optional<int> liveDmxLastIndex_;
    std::vector<int> flashHeld_;  // gedrückte Flash-Looks (liveDown), zuletzt gedrückter hinten
    int flashReturn_{-1};         // Look, zu dem das Loslassen des letzten Flash-Looks zurückkehrt
    void clearFlash() noexcept { flashHeld_.clear(); flashReturn_ = -1; }
    Result<void> armLook(int index);                                  // liveArm ohne clearFlash
    Result<void> goLook(std::optional<double> fadeSeconds);           // liveGo ohne clearFlash
    CueRunner cueRunner_;
    bool cueGo_{false};
    std::vector<std::string> cueKeys_;
    std::uint64_t cueBeats_{0};  // Zahl der Beats beim letzten Blick (Flanke für den Audio-Auslöser)
    std::optional<Error> cueError_;
    bool blackout_{false};
    bool incomplete_{false};  // der Weg zum Ausgang hat eine Lücke: LEDs aus (siehe outputIncomplete)
};

}  // namespace whitehole::runtime
