// canvas_render_stage.hpp: rastert eine Field-Kette pro Frame in eine Canvas-Textur.
//
// Warum es diese Datei gibt: Die Canvas-Stage ist die Brücke Field -> Bild: Das Ergebnis
// ist eine Textur, die spätere Raster-Nodes, die Vorschau und ein Mapping aus dem Bild
// lesen. Sie ist der Einhängepunkt "Graph-Executor" aus render_stage.hpp.
//
// Aufgabe: besitzt die Zieltextur und setzt den Dispatch über die Pixel auf. Den
// gesamten Ablauf pro Frame (CPU-Nodes, Parameter, Pipeline-Lebensdauer, Fehler) erledigt
// der gemeinsame FieldPass (field_pass.hpp); dort steht auch das Fehlerverhalten.
//
// Koordinaten: Pixel (x, y) tastet p = ((x + 0.5) / w, 1 - (y + 0.5) / h, 0) ab. Die
// oberste Zeile liegt also bei p.y nahe 1, wie im Raum (y nach oben). Eine Matrix aus
// wh_mapping trifft damit exakt dieselben Punkte wie die Pixel dieser Textur.
//
// Bekannte Grenzen: nur 2D; die Zeit geht als f32 an den Shader (nach 10^5 s etwa 8 ms
// Auflösung); die Field-Kette hat genau ein Ziel.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "whitehole/core/result.hpp"
#include "whitehole/engine/render_stage.hpp"
#include "whitehole/executor/field_pass.hpp"

namespace whitehole::executor {

struct CanvasStageConfig {
    Extent3 extent{1, 1, 1};  // Tiefe muss 1 sein
    gpu::TextureFormat format{gpu::TextureFormat::Rgba16Float};
    graph::PortRef target;  // Field-Ausgang, der gerastert wird
    std::string label{"wh.canvas"};
};

using CanvasStageStats = StageStats;

class CanvasRenderStage final : public engine::IRenderStage {
public:
    /// Legt Textur und Frame-Puffer an. Es gibt noch keine Pipeline: Die entsteht mit
    /// dem ersten erfolgreichen setGraph().
    /// Fehler: InvalidArgument (Größe 0, ungeeignetes Format), Unsupported (Tiefe > 1),
    /// GpuError. Bei einem Fehler bleibt nichts auf dem Gerät liegen.
    [[nodiscard]] static Result<std::unique_ptr<CanvasRenderStage>> create(
        gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
        const values::ICpuKernelLibrary& kernels, CanvasStageConfig config);

    ~CanvasRenderStage() override;
    CanvasRenderStage(const CanvasRenderStage&) = delete;
    CanvasRenderStage& operator=(const CanvasRenderStage&) = delete;

    /// Siehe FieldPass::setGraph.
    Result<void> setGraph(const graph::Graph& graph) { return pass_->setGraph(graph); }

    void record(const FrameTime& time, gpu::ICommandRecorder& recorder) override;

    /// Die Zieltextur (Storage, Sampled, CopySource).
    [[nodiscard]] gpu::TextureHandle output() const noexcept { return texture_; }

    [[nodiscard]] const std::optional<Error>& graphError() const noexcept { return pass_->graphError(); }
    [[nodiscard]] const std::optional<Error>& frameError() const noexcept { return pass_->frameError(); }
    [[nodiscard]] CanvasStageStats stats() const noexcept { return pass_->stats(); }

private:
    CanvasRenderStage(gpu::IGpuDevice& device, CanvasStageConfig config, gpu::TextureHandle texture,
                      std::unique_ptr<FieldPass> pass);

    gpu::IGpuDevice& device_;
    CanvasStageConfig config_;
    gpu::TextureHandle texture_;
    std::unique_ptr<FieldPass> pass_;
};

}  // namespace whitehole::executor
