// benchmark_gpu_target.hpp: Messziel des Benchmarks auf einem echten (oder Fake-)GPU-Gerät.
//
// Warum es diese Datei gibt (Entwurf docs/entwurf-benchmark.md, Schritt 1): Der Runner (benchmark_runner.hpp) misst nicht
// selbst; dieses Ziel baut je Stufe die passende Render-Stage auf und rendert synchron einzelne Frames. Es benutzt
// dieselben Stages wie die laufende Ausgabe, aber eine eigene, nicht getaktete Schleife ohne Netzwerk, damit die Zahlen
// nur die Rechenleistung zeigen.
//
// - Projektion: CanvasRenderStage in Zielauflösung (offscreen, ohne Fenster, Vollbild oder Vsync). Zurückgelesen wird
//   nur ein Zaun-Puffer von 16 Byte: Eine Projektion bleibt auf der GPU, das Kopieren eines ganzen Bildes zur CPU würde
//   die Messung verfälschen. Das Mappen des Zauns ist erst möglich, wenn alles davor Eingereichte fertig ist.
// - LEDs: SamplingRenderStage auf einer Matrix mit der gewünschten Zahl an LEDs; die Farben werden wie im Betrieb
//   zurückgelesen (die Ausgabe braucht sie auf der CPU).
//
// Zeiten je Frame: CPU = Aufzeichnen und Einreichen (enthält die CPU-Nodes), gesamt = Beginn bis Readback fertig.
// GPU-Zeit liefert dieses Ziel noch nicht (Zeitstempel folgen); `gpuMs` bleibt leer.
// Threads: wie das Gerät, alle Aufrufe von einem Thread.
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include "whitehole/gpu/gpu_device.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/benchmark_runner.hpp"

namespace whitehole::executor {
class CanvasRenderStage;
class SamplingRenderStage;
}  // namespace whitehole::executor

namespace whitehole::runtime {

struct GpuBenchmarkOptions {
    /// So lange wartet frame() höchstens auf den Readback; danach Fehler „GPU antwortet nicht“ (verlorenes Gerät,
    /// hängender Treiber).
    std::chrono::milliseconds readbackTimeout{5000};
    /// Wird zwischen zwei Abfragen des Geräts aufgerufen (Standard: kurz abgeben). Tests zählen damit die Wartezeit.
    std::function<void()> waitStep;
};

class GpuBenchmarkTarget final : public IBenchmarkTarget {
public:
    GpuBenchmarkTarget(gpu::IGpuDevice& device, const nodes::BuiltinLibrary& library, GpuBenchmarkOptions options = {});
    ~GpuBenchmarkTarget() override;
    GpuBenchmarkTarget(const GpuBenchmarkTarget&) = delete;
    GpuBenchmarkTarget& operator=(const GpuBenchmarkTarget&) = delete;

    /// Fehler: die der Stage (GpuError, ShaderError, InvalidArgument) und der Szene; die vorige Stufe ist danach schon
    /// abgebaut.
    Result<void> prepare(const BenchmarkStage& stage) override;
    /// Fehler: Unsupported vor prepare, GpuError (Einreichen, Readback, Zeitüberschreitung), Fehler des Frames.
    Result<BenchmarkFrame> frame() override;

private:
    void release() noexcept;

    gpu::IGpuDevice& device_;
    const nodes::BuiltinLibrary& library_;
    GpuBenchmarkOptions options_;
    std::unique_ptr<executor::CanvasRenderStage> canvas_;
    std::unique_ptr<executor::SamplingRenderStage> sampling_;
    gpu::BufferHandle fence_{};
    gpu::BufferHandle readback_{};
    std::uint64_t readbackBytes_{0};
    gpu::BufferHandle copySource_{};
    std::uint64_t frameIndex_{0};
    double seconds_{0.0};
};

}  // namespace whitehole::runtime
