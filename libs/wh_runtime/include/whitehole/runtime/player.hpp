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

    /// Schreibt das Projekt (Layout, Graph, Ausgabe samt aktueller Parameter) als Datei und merkt
    /// sich den Pfad. Kommentare einer von Hand geschriebenen Datei gehen dabei verloren.
    Result<void> save(const std::string& path);

    /// Bild der LEDs: während des Laufs mit den Farben des letzten Frames (schwarz, solange noch
    /// keiner da ist), sonst das Layout. Fehler: InvalidArgument, wenn nichts geladen ist.
    [[nodiscard]] Result<preview::Image> image(const preview::PreviewOptions& options) const;

private:
    Result<void> adopt(project::Project project, std::string path);

    gpu::IGpuDevice* device_;
    const nodes::BuiltinLibrary& library_;
    output::IPacketSink& sink_;

    std::optional<project::Project> project_;
    std::string path_;
    mapping::LedMap map_;  // des geladenen Projekts, für die Layout-Vorschau
    std::unique_ptr<engine::SteadyFrameClock> clock_;
    std::unique_ptr<Session> session_;
    std::optional<Error> stepError_;
    bool graphDirty_{false};  // Parameter geändert, Session noch nicht informiert
};

}  // namespace whitehole::runtime
