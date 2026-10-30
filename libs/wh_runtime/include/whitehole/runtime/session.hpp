// session.hpp: ein Projekt im Betrieb.
//
// Warum es diese Datei gibt: Layout, Effekt und Ausgabe sind getrennte Bausteine mit klaren
// Schnittstellen. Irgendwer muss sie in der richtigen Reihenfolge anlegen, verbinden und in
// der richtigen Reihenfolge wieder abbauen. Das ist die Aufgabe der Session; Konsolen-Runner
// und GUI benutzen dieselbe Zusammenstellung.
//
// Zwei Stufen:
//   - buildOutput: Layout backen, Ausgabeplan und Pipeline bauen. Braucht keine GPU und
//     deckt alle Konfigurationsfehler beim Patchen auf ("--check" des Runners).
//   - Session::create: zusätzlich Abtast-Stage, Readback-Ring und Engine. Braucht ein Gerät.
//
// Ablauf pro Frame: renderFrame() -> Engine (Stage auf dem Gerät, Readback) -> Mailbox ->
// Ausgabe-Thread (Farben umrechnen, Pakete bauen) -> IPacketSink.
//
// Lebensdauer: Gerät, Uhr, Bibliotheken und Sink müssen die Session überleben.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

#include "whitehole/compiler/node_definition.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/services.hpp"
#include "whitehole/engine/readback_ring.hpp"
#include "whitehole/engine/render_engine.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/gpu/gpu_device.hpp"
#include "whitehole/mapping/led_map.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/output/output_worker.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/values/cpu_kernel.hpp"

namespace whitehole::runtime {

struct OutputParts {
    mapping::LedMap map;
    std::unique_ptr<output::OutputPipeline> pipeline;
};

/// Fehler: InvalidArgument (Layout leer, nichts gepatcht, Ziel fehlt), sonst die Fehler von
/// Layout::bake, OutputPlan::create und OutputPipeline::create.
[[nodiscard]] Result<OutputParts> buildOutput(const project::Project& project, output::IPacketSink& sink);

struct SessionDeps {
    gpu::IGpuDevice& device;
    const compiler::ISnippetLibrary& snippets;
    const values::ICpuKernelLibrary& kernels;
    output::IPacketSink& sink;
    IFrameClock& clock;
};

struct SessionStats {
    engine::EngineStats engine;
    output::OutputStats output;
    std::uint64_t mailboxDrops{0};   // Frames, die der Ausgabe-Thread nicht mehr rechtzeitig abholte
    std::uint64_t frameErrors{0};    // Frames, die die Pipeline ablehnte
    std::uint64_t failedReadbacks{0};
};

class Session {
public:
    /// Fehler: wie buildOutput, dazu die der Abtast-Stage (ShaderError, GpuError) bei einem
    /// Graphen, der sich nicht übersetzen lässt, und InvalidArgument, wenn `project.target` fehlt.
    [[nodiscard]] static Result<std::unique_ptr<Session>> create(const project::Project& project,
                                                                 SessionDeps deps,
                                                                 std::size_t readbackSlots = 3);

    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /// Ein Frame. Siehe RenderEngine::renderFrame.
    Result<void> renderFrame() { return engine_->renderFrame(); }

    /// Pollt das Gerät, bis alle Readbacks ausgeliefert und vom Ausgabe-Thread verarbeitet sind
    /// (oder das Timeout abläuft). Liefert, ob alles fertig wurde.
    bool flush(std::chrono::milliseconds timeout);

    [[nodiscard]] std::uint32_t ledCount() const noexcept { return stage_->ledCount(); }
    [[nodiscard]] SessionStats stats() const;
    [[nodiscard]] std::optional<Error> lastOutputError() const;
    /// Fehler der Abtast-Stage (Graph nicht übersetzbar, Frame fehlgeschlagen).
    [[nodiscard]] std::optional<Error> stageError() const;

private:
    Session(gpu::IGpuDevice& device, std::unique_ptr<output::OutputWorker> worker,
            std::shared_ptr<executor::SamplingRenderStage> stage,
            std::unique_ptr<engine::ReadbackRing> ring, std::unique_ptr<engine::RenderEngine> engine,
            std::size_t slots);

    gpu::IGpuDevice& device_;
    std::unique_ptr<output::OutputWorker> worker_;           // zuerst anlegen, zuletzt entfernen
    std::shared_ptr<executor::SamplingRenderStage> stage_;
    std::unique_ptr<engine::ReadbackRing> ring_;
    std::unique_ptr<engine::RenderEngine> engine_;           // zuletzt angelegt, zuerst entfernt
    std::size_t slots_;
};

}  // namespace whitehole::runtime
