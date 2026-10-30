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
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/packet_sink.hpp"
#include "whitehole/preview/preview.hpp"
#include "whitehole/project/project.hpp"
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
    /// Entfernt Node und Verbindungen. Der Node, an dem `output` hängt, lässt sich nicht
    /// entfernen (InvalidArgument): Setze zuerst mit setTarget einen anderen Ausgang.
    Result<void> removeNode(NodeId node);
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

    /// Bild der LEDs: während des Laufs mit den Farben des letzten Frames (schwarz, solange noch
    /// keiner da ist), sonst das Layout. Fehler: InvalidArgument, wenn nichts geladen ist.
    [[nodiscard]] Result<preview::Image> image(const preview::PreviewOptions& options) const;

private:
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
    bool rebuildDirty_{false};  // Ausgang, Layout oder Ausgabe geändert: Session neu aufbauen
    bool modified_{false};     // seit Laden/Speichern verändert
    struct Snapshot {
        project::Project project;
        bool rebuild{false};  // die Änderung zwischen diesem und dem nächsten Stand brauchte eine neue Session
    };
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    std::string lastEditKey_;  // zum Zusammenfassen gleichartiger Änderungen
};

}  // namespace whitehole::runtime
