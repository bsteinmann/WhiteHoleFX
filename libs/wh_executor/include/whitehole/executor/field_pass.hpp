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
    Result<void> setGraph(const graph::Graph& graph);

    /// Wertet aus, lädt hoch und zeichnet den Dispatch auf. `consumerBindings` kommen
    /// zu den beiden Bindungen der Feldbibliothek (Gruppe 0) hinzu.
    /// Liefert true, wenn ein Dispatch aufgezeichnet wurde.
    bool record(const FrameTime& time, gpu::ICommandRecorder& recorder,
                std::span<const gpu::Binding> consumerBindings, const DispatchSize& size);

    [[nodiscard]] bool ready() const noexcept { return pipeline_.has_value() && graph_.has_value(); }
    [[nodiscard]] const std::optional<Error>& graphError() const noexcept { return graphError_; }
    [[nodiscard]] const std::optional<Error>& frameError() const noexcept { return frameError_; }
    [[nodiscard]] StageStats stats() const noexcept { return stats_; }

private:
    FieldPass(gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
              const values::ICpuKernelLibrary& kernels, graph::PortRef target, std::string label,
              EntryFn entry, gpu::BufferHandle frameBuffer);

    Result<void> rebuildPipeline(const compiler::CompiledField& compiled);
    void releasePipeline() noexcept;

    gpu::IGpuDevice& device_;
    const compiler::ISnippetLibrary& snippets_;
    values::ValueEvaluator evaluator_;
    values::ValueStore store_;
    graph::PortRef target_;
    std::string label_;
    EntryFn entry_;

    gpu::BufferHandle frameBuffer_;

    std::optional<graph::Graph> graph_;
    compiler::ParameterLayout layout_;
    std::uint64_t structureHash_{0};
    std::optional<gpu::ComputePipelineHandle> pipeline_;
    std::optional<gpu::BufferHandle> parameterBuffer_;
    std::vector<gpu::Binding> bindings_;  // wiederverwendet, damit record() nicht allokiert

    std::optional<Error> graphError_;
    std::optional<Error> frameError_;
    StageStats stats_;
};

}  // namespace whitehole::executor
