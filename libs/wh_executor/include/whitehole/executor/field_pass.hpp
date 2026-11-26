// field_pass.hpp: der gemeinsame Ablauf aller Pässe, die ein Feld auswerten.
//
// Warum es diese Datei gibt: Der Canvas-Pass (Feld -> Textur) und der Abtast-Pass
// (Feld -> LED-Farben) unterscheiden sich nur in zwei Dingen: dem Compute-Einstieg, der
// an die Feldbibliothek gehängt wird, und den Ressourcen in Bind-Group 1. Alles andere
// ist identisch und darf nur einmal existieren: CPU-Nodes auswerten, Parameter packen
// und hochladen, Frame-Daten hochladen, Pipeline nur bei Strukturänderung neu bauen,
// bei Fehlern den letzten guten Zustand behalten.
//
// Aufgabe: Besitzt Evaluator, Wertespeicher, Frame-Puffer, Parameterpuffer und
// Pipeline. Der Konsument liefert beim Erzeugen eine Funktion, die den Einstieg an die
// Bibliothek hängt, und übergibt pro Frame seine eigenen Bindungen und die Größe des
// Dispatches.
//
// Pipeline-Lebensdauer: Eine neue Pipeline entsteht nur, wenn sich der Shadertext
// (structureHash) ändert. Parameter, Modulation und Zeit ändern nur Pufferinhalte.
// Beim Umbau entsteht zuerst alles Neue; erst danach wird das Alte freigegeben.
//
// Fehlerverhalten: Scheitert setGraph (Graph ungültig, Compiler-, Pipeline- oder
// Pufferfehler), bleibt der letzte gute Zustand aktiv und rendert weiter. Der Fehler
// steht in graphError().
//
// Übersetzen im Hintergrund (setBackgroundCompile, im Live-Betrieb an): Ändert sich die Struktur, läuft die bisherige
// Pipeline mit dem bisherigen Graphen weiter, bis der Treiber die neue übersetzt hat (IGpuDevice::beginComputePipeline);
// record() schaltet dann um. Zuletzt benutzte Pipelines bleiben in einem kleinen Zwischenspeicher, ein Wechsel zurück zu
// einer schon übersetzten Struktur ist dann sofort da. Ohne vorherige Pipeline wird immer sofort übersetzt.
// Vorab übersetzen (prefetch): Der Aufrufer nennt Graphen, die vielleicht bald kommen (Live-Betrieb: die Felder des
// laufenden Looks, Rückmeldung vom 26.11.: das erste Drücken eines Flash stockte). Ihre Pipelines entstehen nachrangig
// und landen im Zwischenspeicher; ein noch laufender Vorab-Auftrag wird bei Bedarf vorgezogen statt doppelt übersetzt.
//
// Threading: alles auf dem Engine-Thread. Gerät und Bibliotheken müssen den Pass überleben.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/services.hpp"
#include "whitehole/gpu/gpu_device.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/values/cpu_kernel.hpp"
#include "whitehole/values/value_evaluator.hpp"
#include "whitehole/values/value_store.hpp"

namespace whitehole::executor {

struct StageStats {
    std::uint64_t pipelinesBuilt{0};
    std::uint64_t framesRecorded{0};
    std::uint64_t framesSkipped{0};  // ohne gültigen Graphen oder wegen eines Upload-Fehlers
};

struct DispatchSize {
    std::uint32_t x{1};
    std::uint32_t y{1};
    std::uint32_t z{1};
};

class FieldPass {
public:
    /// Hängt den Compute-Einstieg `main` an die Feldbibliothek.
    using EntryFn = std::function<Result<std::string>(std::string_view fieldLibrary)>;

    /// Legt den Frame-Puffer an. Es gibt noch keine Pipeline: Die entsteht mit dem
    /// ersten erfolgreichen setGraph(). Fehler: GpuError.
    [[nodiscard]] static Result<std::unique_ptr<FieldPass>> create(
        gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
        const values::ICpuKernelLibrary& kernels, graph::PortRef target, std::string label,
        EntryFn entry);

    ~FieldPass();
    FieldPass(const FieldPass&) = delete;
    FieldPass& operator=(const FieldPass&) = delete;

    /// Übernimmt einen Snapshot des Graphen; baut die Pipeline nur bei neuer Struktur.
    /// Bei einem Fehler bleibt der bisherige Zustand bestehen.
    /// `extras`: zusätzliche Field-Ausgänge, die mit übersetzt werden (Einstiege `wh_field_extra<i>`, siehe compileField);
    /// sie gelten, bis setGraph sie erneut setzt.
    Result<void> setGraph(const graph::Graph& graph, std::span<const graph::PortRef> extras = {});

    /// Schnellweg für den Fall, dass sich nur Parameterwerte geändert haben (Regler, Überblendung): übernimmt
    /// die Werte, ohne den Graphen neu zu übersetzen. Der Aufrufer muss sicher sein, dass sich weder Nodes
    /// noch Kabel noch Textparameter geändert haben; im Zweifel setGraph() benutzen. Ohne bisherigen
    /// erfolgreichen setGraph() fällt es auf diesen zurück.
    Result<void> setParameters(const graph::Graph& graph);

    /// Wertet aus, lädt hoch und zeichnet den Dispatch auf. `consumerBindings` kommen
    /// zu den beiden Bindungen der Feldbibliothek (Gruppe 0) hinzu.
    /// Liefert true, wenn ein Dispatch aufgezeichnet wurde.
    bool record(const FrameTime& time, gpu::ICommandRecorder& recorder,
                std::span<const gpu::Binding> consumerBindings, const DispatchSize& size);

    /// Meldet dem Shader, ob die LED-Karte räumlich ist (WhFrame.volume, Platzhalter @volume). Standard: nein.
    void setVolume(bool volume) noexcept { volume_ = volume; }

    /// Meldet dem Shader die echten Seitenverhältnisse der Leinwand (WhFrame.aspect/aspectZ, Platzhalter
    /// @aspect/@aspectZ). `aspectXY` = Breite/Höhe, `aspectXZ` = Breite/Tiefe (nur für Volume-Nodes
    /// relevant). Standard: beide 1 (quadratisch/flach), wie vor Einführung dieser Werte.
    void setAspect(float aspectXY, float aspectXZ) noexcept {
        aspect_ = aspectXY;
        aspectZ_ = aspectXZ;
    }

    /// Meldet dem Shader die echte Zahl der LEDs über Breite/Höhe (WhFrame.columns/rows, Platzhalter
    /// @columns/@rows). 0 = unbekannt. Standard: beide 0 (unbekannt), wie vor Einführung dieser Werte.
    void setResolution(float columns, float rows) noexcept {
        columns_ = columns;
        rows_ = rows;
    }

    /// Vorab übersetzen (nur im Hintergrundmodus und mit laufender Pipeline, sonst nichts): Die Pipeline zu `graph`
    /// entsteht nachrangig (IGpuDevice::prefetchComputePipeline) und kommt in den Zwischenspeicher. Eine schon bekannte
    /// Struktur (laufend, wartend, zwischengespeichert, bestellt) kostet nur das Übersetzen in WGSL.
    /// Fehler: die von compileField und vom Gerät (der Graph wird übergangen, der laufende Zustand bleibt).
    Result<void> prefetch(const graph::Graph& graph, std::span<const graph::PortRef> extras = {});
    /// Verwirft alle offenen Vorab-Aufträge (z. B. weil ein anderer Look läuft). Fertige bleiben im Zwischenspeicher.
    void cancelPrefetches() noexcept;
    /// Offene Vorab-Aufträge (für Tests).
    [[nodiscard]] std::size_t prefetchesOpen() const noexcept { return prefetches_.size(); }

    [[nodiscard]] bool ready() const noexcept { return pipeline_.has_value() && graph_.has_value(); }
    /// Übersetzen im Hintergrund und Zwischenspeicher an/aus (Standard: aus, setGraph übersetzt sofort).
    void setBackgroundCompile(bool on) noexcept { background_ = on; }
    /// Wartet eine neue Struktur noch auf den Treiber? Solange zeigt der Pass den bisherigen Graphen.
    [[nodiscard]] bool pending() const noexcept { return pending_.has_value(); }
    [[nodiscard]] const std::optional<Error>& graphError() const noexcept { return graphError_; }
    [[nodiscard]] const std::optional<Error>& frameError() const noexcept { return frameError_; }
    [[nodiscard]] StageStats stats() const noexcept { return stats_; }
    /// Die Werte, die die CPU-Nodes im letzten Frame geliefert haben (nur vom Frame-Thread aus lesen).
    [[nodiscard]] const values::ValueStore& values() const noexcept { return store_; }

private:
    FieldPass(gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
              const values::ICpuKernelLibrary& kernels, graph::PortRef target, std::string label,
              EntryFn entry, gpu::BufferHandle frameBuffer);

    Result<void> rebuildPipeline(const compiler::CompiledField& compiled);
    /// Setzt eine fertige Pipeline ein (Parameterpuffer passend); die bisherige kommt in den Zwischenspeicher (Hintergrund-
    /// modus) oder wird freigegeben. Bei einem Fehler wird `pipeline` freigegeben und der alte Zustand bleibt.
    Result<void> installPipeline(gpu::ComputePipelineHandle pipeline, const compiler::CompiledField& compiled, bool fresh);
    void releasePipeline() noexcept;
    void cancelPending() noexcept;
    void pollPending();     // in record(): fertige Hintergrund-Pipeline einsetzen
    void pollPrefetches();  // in record(): fertige Vorab-Pipelines in den Zwischenspeicher
    [[nodiscard]] std::optional<gpu::ComputePipelineHandle> takeCached(std::uint64_t hash) noexcept;
    /// Legt eine Pipeline in den Zwischenspeicher (die älteste fällt bei Überlauf heraus, ein Duplikat wird freigegeben).
    void remember(std::uint64_t hash, gpu::ComputePipelineHandle pipeline) noexcept;
    /// Läuft, wartet, liegt im Zwischenspeicher oder ist vorab bestellt?
    [[nodiscard]] bool known(std::uint64_t hash) const noexcept;
    /// Übersetzt einen Graphen wie setGraph (mit den unsichtbaren Addern aus output_merge.hpp).
    struct Compiled {
        compiler::CompiledField field;
        graph::Graph graph;
    };
    [[nodiscard]] Result<Compiled> compile(const graph::Graph& graph, std::span<const graph::PortRef> extras) const;

    gpu::IGpuDevice& device_;
    const compiler::ISnippetLibrary& snippets_;
    values::ValueEvaluator evaluator_;
    values::ValueStore store_;
    graph::PortRef target_;
    std::string label_;
    EntryFn entry_;

    gpu::BufferHandle frameBuffer_;
    bool volume_{false};
    float aspect_{1.0F};
    float aspectZ_{1.0F};
    float columns_{0.0F};
    float rows_{0.0F};

    std::optional<graph::Graph> graph_;
    compiler::ParameterLayout layout_;
    std::uint64_t structureHash_{0};
    std::optional<gpu::ComputePipelineHandle> pipeline_;
    std::optional<gpu::BufferHandle> parameterBuffer_;

    struct Pending {
        gpu::PipelineTicket ticket;
        compiler::CompiledField compiled;
        graph::Graph graph;
    };
    struct Cached {
        std::uint64_t hash{0};
        gpu::ComputePipelineHandle pipeline;
    };
    struct Prefetch {
        std::uint64_t hash{0};
        gpu::PipelineTicket ticket;
    };
    // Groß genug für alle Felder eines Live-Rasters über dem laufenden Look plus die zuletzt benutzten Kombinationen.
    static constexpr std::size_t kCacheSize = 48;
    bool background_{false};
    std::optional<Pending> pending_;
    std::vector<Prefetch> prefetches_;  // vorab bestellt, noch nicht abgeholt
    std::vector<Cached> cache_;  // zuletzt benutzte Pipelines, die neueste hinten
    std::vector<gpu::Binding> bindings_;  // wiederverwendet, damit record() nicht allokiert

    std::optional<Error> graphError_;
    std::optional<Error> frameError_;
    StageStats stats_;
};

}  // namespace whitehole::executor
