// benchmark_on_device_test.cpp: der Benchmark auf einem echten Gerät (oder dem Software-Adapter), ohne Zeit-Schwellwerte.
//
// Warum es die Datei gibt: Die Szenen müssen sich auf einer echten GPU übersetzen und rendern lassen, und der Zaun-
// Readback der Projektion muss wirklich auf das Ende der Arbeit warten. Grenzwerte gibt es keine: CI-Runner sind geteilt
// und haben keine GPU (Entwurf docs/entwurf-benchmark.md, Abschnitt Tests). Ohne Adapter übersprungen.
//
// Jede Kombination aus Zeitstempel (an/aus), Szene und Reihe läuft einzeln (GENERATE): Stürzt ein Adapter ab, nennt Catch2
// im Kopf der Meldung, welche Kombination es war, statt nur „der Test“. Das war nötig, als der Windows-Software-Adapter
// (D3D12/WARP) in der CI abstürzte und aus dem Protokoll nicht hervorging, ob Zeitstempel, Messziel oder Szene schuld waren.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "gpu_stage_support.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/benchmark_gpu_target.hpp"
#include "whitehole/runtime/benchmark_report.hpp"

using namespace whitehole;
using namespace whitehole::runtime;

TEST_CASE("Benchmark auf dem Gerät: jede Szene misst in beiden Reihen", "[gpu_device][benchmark]") {
    const bool timestamps = GENERATE(false, true);
    const BenchmarkScene scene = GENERATE(BenchmarkScene::Plasma, BenchmarkScene::Particles, BenchmarkScene::Maze,
                                          BenchmarkScene::Layers);
    const BenchmarkKind kind = GENERATE(BenchmarkKind::Projection, BenchmarkKind::Leds);
    INFO("Zeitstempel " << (timestamps ? "an" : "aus") << ", Szene " << benchmarkSceneId(scene) << ", Reihe "
                        << (kind == BenchmarkKind::Projection ? "Projektion" : "LEDs"));

    gpu::WebGpuOptions options;
    options.timestamps = timestamps;
    auto made = gpu::WebGpuDevice::create(options);
    if (!made.ok()) {
        options.forceFallbackAdapter = true;
        made = gpu::WebGpuDevice::create(options);
    }
    if (!made.ok()) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const auto device = std::move(made.value());
    INFO("Adapter " << device->deviceName() << " [" << device->backendName() << "], GPU-Zeit "
                    << (device->measuresGpuTime() ? "ja" : "nein"));
    auto lib = nodes::BuiltinLibrary::create();
    REQUIRE(lib.ok());
    GpuBenchmarkTarget target{*device, *lib.value()};
    std::vector<BenchmarkStage> plan;
    if (kind == BenchmarkKind::Projection) {
        plan.push_back({BenchmarkKind::Projection, scene, 480, 270, 0});
    } else {
        plan.push_back({BenchmarkKind::Leds, scene, 0, 0, 1'000});
    }
    BenchmarkLimits limits;
    limits.warmupSeconds = 0.05;
    limits.measureSeconds = 0.2;
    const BenchmarkRun run = runBenchmark(plan, target, limits);
    INFO(run.abortReason);
    REQUIRE_FALSE(run.aborted);
    for (const BenchmarkStageResult& r : run.stages) {
        INFO(benchmarkStageLabel(r.stage) << " " << benchmarkSceneId(r.stage.scene));
        CHECK((r.outcome == BenchmarkOutcome::Ok || r.outcome == BenchmarkOutcome::OverBudget));
        CHECK(r.stats.frames >= limits.minFrames);
        CHECK(r.stats.medianMs > 0.0);
        CHECK(r.stats.medianMs >= r.stats.cpuMedianMs);
        if (device->measuresGpuTime()) {
            REQUIRE(r.stats.gpuMedianMs.has_value());  // jeder Frame hat eine GPU-Zeit
            CHECK(*r.stats.gpuMedianMs > 0.0);
            CHECK(*r.stats.gpuMedianMs <= r.stats.maxMs + 1.0);
        }
    }
    UNSCOPED_INFO(benchmarkMarkdown(run, BenchmarkSystemInfo{device->deviceName(), device->backendName(), device->driverInfo(), "test", ""}, limits));
}
