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
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/gpu/gpu_device.hpp"
#include "whitehole/mapping/region_edit.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/packet_sink.hpp"
#include "whitehole/preview/preview.hpp"
#include "whitehole/preview/view3d.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/audio_usage.hpp"
#include "whitehole/runtime/dmx_usage.hpp"
#include "whitehole/runtime/led_info.hpp"
#include "whitehole/runtime/live_decks.hpp"
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
    /// Ein Eingang mit demselben Schlüssel ist verbunden: Der Wert kommt aus dem Graphen, ein
    /// Regler hätte keine Wirkung.
    bool driven{false};
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
        : device_{device}, library_{library}, sink_{sink} {}
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
    /// Lage im Editor; ändert den Effekt nicht.
    Result<void> moveNode(NodeId node, Vec2 position);
    /// Wählt den Field-Ausgang, der auf die LEDs abgetastet wird. Läuft das Projekt, wird es im
    /// nächsten step() neu aufgebaut (kurze Unterbrechung der Ausgabe).
    /// Fehler: NotFound (Node oder Port), IncompatiblePorts (kein Field-Ausgang).
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
    /// Verschiebt mehrere Fixtures um denselben Versatz (ein Undo-Schritt, wie ein Ziehen mit der Maus).
    /// Fehler: NotFound bei unbekannter Id, InvalidArgument bei nicht endlichem Versatz.
    Result<void> moveFixtures(const std::vector<mapping::FixtureId>& ids, const Vec3& delta);

    /// Ausrichten an der Gesamtbox der gewählten Fixtures. Oben/unten und links/rechts sind die
    /// Richtungen der Vorschau (y nach oben, x nach rechts).
    enum class Align : std::uint8_t { Left, HCenter, Right, Top, VCenter, Bottom };
    /// Achse für gleichmäßiges Verteilen.
    enum class Axis : std::uint8_t { X, Y, Z };
    /// Richtet die Fixtures aus (ein Undo-Schritt). Fehler: NotFound (unbekannte Id), InvalidArgument
    /// (Liste leer).
    Result<void> alignFixtures(const std::vector<mapping::FixtureId>& ids, Align mode);
    /// Verteilt die Fixtures so, dass die Lücken zwischen ihren Boxen auf der Achse gleich groß sind;
    /// das erste und das letzte (nach Lage sortiert) bleiben stehen. Fehler: InvalidArgument bei
    /// weniger als drei Fixtures, NotFound bei unbekannter Id.
    Result<void> distributeFixtures(const std::vector<mapping::FixtureId>& ids, Axis axis);

    /// Die Box der ganzen Wand (alle Fixtures) in Weltkoordinaten. Ohne Projekt eine leere Box.
    [[nodiscard]] mapping::Box wallBox() const noexcept { return map_.volume; }

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

    /// Ein Patch mit dem ersten freien Universe hinter allen Art-Net/sACN-Patches auf Ziel 0
    /// (Art-Net, Reihenfolge RGB). `leds` ist die Zahl der LEDs des neuen Fixtures; der Vorschlag
    /// geht davon aus, dass je Universe 170 LEDs passen. Ohne Projekt Universe 0.
    [[nodiscard]] output::FixturePatch suggestPatch(std::uint64_t leds) const;

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
    /// Blackout: alle LEDs aus, die Pakete laufen weiter (Controller bleiben verbunden).
    void setBlackout(bool on);
    [[nodiscard]] bool blackout() const noexcept { return blackout_; }

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
    /// Uhr für die Überblendung in Sekunden (Standard: Systemuhr). Nur für Tests austauschbar.
    void setLiveClock(std::function<double()> clock) { liveClock_ = std::move(clock); }

    /// Die LED unter einem Bildpunkt der Vorschau (`frame` ist der Ausschnitt des gezeichneten Bildes)
    /// samt Fixture, Nummer am Kabel, Adresse und Farbe. Leer, wenn keine LED näher als `maxDistance`
    /// Pixel liegt oder nichts geladen ist.
    [[nodiscard]] std::optional<LedInfo> ledInfoAt(const preview::ViewFrame& frame, float pixelX, float pixelY,
                                                   float maxDistance = 10.0F) const;

    /// Welche DMX-Universen die DMX-Nodes des geladenen Projekts lesen (leer, wenn nichts geladen ist).
    [[nodiscard]] DmxUsage dmxUsage() const;
    /// Wie viele Audio-Nodes das geladene Projekt hat (0, wenn nichts geladen ist).
    [[nodiscard]] AudioUsage audioUsage() const;

    /// Bild der LEDs: während des Laufs mit den Farben des letzten Frames (schwarz, solange noch
    /// keiner da ist), sonst das Layout. Fehler: InvalidArgument, wenn nichts geladen ist.
    [[nodiscard]] Result<preview::Image> image(const preview::PreviewOptions& options) const;

    /// Dasselbe durch eine frei drehbare Kamera (3D-Vorschau, nur zum Ansehen). Fehler wie `image`.
    [[nodiscard]] Result<preview::Image> image3d(const preview::Camera3d& camera,
                                                 const preview::PreviewOptions& options) const;

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
    [[nodiscard]] double liveNow() const {
        return liveClock_ ? liveClock_()
                          : std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NodeDefaults nodeDefaults_;
    gpu::IGpuDevice* device_;
    const nodes::BuiltinLibrary& library_;
    output::IPacketSink& sink_;

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
    bool blackout_{false};
};

}  // namespace whitehole::runtime
