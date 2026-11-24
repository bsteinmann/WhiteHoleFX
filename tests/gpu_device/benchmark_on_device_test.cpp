// benchmark_on_device_test.cpp: der Benchmark auf einem echten Gerät (oder dem Software-Adapter), ohne Zeit-Schwellwerte.
//
// Warum es die Datei gibt: Die Szenen müssen sich auf einer echten GPU übersetzen und rendern lassen, und der Zaun-
// Readback der Projektion muss wirklich auf das Ende der Arbeit warten. Grenzwerte gibt es keine: CI-Runner sind geteilt
// und haben keine GPU (Entwurf docs/entwurf-benchmark.md, Abschnitt Tests). Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "gpu_stage_support.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/benchmark_gpu_target.hpp"
#include "whitehole/runtime/benchmark_report.hpp"

using namespace whitehole;
using namespace whitehole::runtime;

TEST_CASE("Benchmark auf dem Gerät: jede Szene misst in beiden Reihen", "[gpu_device][benchmark]") {
    const auto device = gputest::makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto lib = nodes::BuiltinLibrary::create();
    REQUIRE(lib.ok());
    GpuBenchmarkTarget target{*device, *lib.value()};
    std::vector<BenchmarkStage> plan;
    for (const BenchmarkScene scene :
         {BenchmarkScene::Plasma, BenchmarkScene::Particles, BenchmarkScene::Maze, BenchmarkScene::Layers}) {
        plan.push_back({BenchmarkKind::Projection, scene, 480, 270, 0});
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
    }
    UNSCOPED_INFO(benchmarkMarkdown(run, BenchmarkSystemInfo{device->adapterName(), "", "", "test", ""}, limits));
}
