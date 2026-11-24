// benchmark_gpu_target_test.cpp: das GPU-Messziel des Benchmarks auf dem Fake-Gerät.
//
// Warum es die Datei gibt: Aufbau aller Szenen in beiden Reihen, Zeitüberschreitung beim Readback (hängende GPU),
// Fehler beim Einreichen und das Zusammenspiel mit dem Runner sollen ohne echte GPU geprüft sein. Ob die Szenen auf
// einem echten Gerät laufen, prüft tests/gpu_device/benchmark_on_device_test.cpp.
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "fake_gpu_device.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/benchmark_gpu_target.hpp"
#include "whitehole/runtime/benchmark_scenes.hpp"

using namespace whitehole;
using namespace whitehole::runtime;

namespace {

std::unique_ptr<nodes::BuiltinLibrary> library() {
    auto lib = nodes::BuiltinLibrary::create();
    REQUIRE(lib.ok());
    return std::move(lib.value());
}

}  // namespace

TEST_CASE("Benchmark-Szenen: alle vier lassen sich aufbauen", "[benchmark]") {
    const auto lib = library();
    for (const BenchmarkScene scene :
         {BenchmarkScene::Plasma, BenchmarkScene::Particles, BenchmarkScene::Maze, BenchmarkScene::Layers}) {
        auto built = makeBenchmarkScene(scene, lib->registry);
        REQUIRE(built.ok());
        CHECK(built.value().output.portKey == "out");
        CHECK(built.value().graph.nodes().size() == (scene == BenchmarkScene::Layers ? 6U : 1U));
    }
}

TEST_CASE("Benchmark-GPU-Ziel: Projektion und LEDs aufbauen und messen", "[benchmark]") {
    const auto lib = library();
    test::FakeGpuDevice device;
    GpuBenchmarkTarget target{device, *lib};
    CHECK(target.frame().error().code == ErrorCode::Unsupported);  // noch keine Stufe
    for (const BenchmarkScene scene :
         {BenchmarkScene::Plasma, BenchmarkScene::Particles, BenchmarkScene::Maze, BenchmarkScene::Layers}) {
        INFO(benchmarkSceneId(scene));
        REQUIRE(target.prepare({BenchmarkKind::Projection, scene, 480, 270, 0}).ok());
        auto frame = target.frame();
        REQUIRE(frame.ok());
        CHECK(frame.value().totalMs >= frame.value().cpuMs);
        CHECK_FALSE(frame.value().gpuMs.has_value());
        REQUIRE(target.prepare({BenchmarkKind::Leds, scene, 0, 0, 1'000}).ok());
        REQUIRE(target.frame().ok());
    }
}

TEST_CASE("Benchmark-GPU-Ziel: hängende GPU und Fehler beim Einreichen", "[benchmark]") {
    const auto lib = library();
    test::FakeGpuDevice device;
    int waits = 0;
    GpuBenchmarkOptions options;
    options.readbackTimeout = std::chrono::milliseconds{30};
    options.waitStep = [&waits] { ++waits; };
    GpuBenchmarkTarget target{device, *lib, options};
    REQUIRE(target.prepare({BenchmarkKind::Leds, BenchmarkScene::Plasma, 0, 0, 1'000}).ok());

    device.holdReadbacks(true);
    auto hung = target.frame();
    REQUIRE_FALSE(hung.ok());
    CHECK(hung.error().code == ErrorCode::GpuError);
    CHECK(hung.error().message == "GPU antwortet nicht");
    CHECK(waits > 0);
    CHECK(target.frame().error().code == ErrorCode::Unsupported);  // Stufe abgebaut, muss neu vorbereitet werden
    device.holdReadbacks(false);
    device.poll();  // der verspätete Rückruf darf nichts mehr anrichten

    REQUIRE(target.prepare({BenchmarkKind::Projection, BenchmarkScene::Maze, 480, 270, 0}).ok());
    device.failNextSubmit();
    CHECK_FALSE(target.frame().ok());
    CHECK(target.frame().ok());
}

TEST_CASE("Benchmark-GPU-Ziel zusammen mit dem Runner: Schnelltest läuft durch", "[benchmark]") {
    const auto lib = library();
    test::FakeGpuDevice device;
    GpuBenchmarkTarget target{device, *lib};
    BenchmarkLimits limits;
    limits.warmupSeconds = 0.0;
    limits.measureSeconds = 0.0;  // nur die Mindestzahl an Frames
    const BenchmarkRun run = runBenchmark(benchmarkPlan(BenchmarkSeries::Leds, BenchmarkMode::Quick), target, limits);
    REQUIRE_FALSE(run.aborted);
    for (const BenchmarkStageResult& r : run.stages) {
        CHECK(r.outcome == BenchmarkOutcome::Ok);
        CHECK(r.stats.frames == limits.minFrames);
    }
}

TEST_CASE("Benchmark-GPU-Ziel: GPU-Zeit kommt vom Gerät, sonst bleibt sie leer", "[benchmark]") {
    const auto lib = library();
    test::FakeGpuDevice device;
    GpuBenchmarkTarget target{device, *lib};
    REQUIRE(target.prepare({BenchmarkKind::Leds, BenchmarkScene::Plasma, 0, 0, 1'000}).ok());
    CHECK_FALSE(target.frame().value().gpuMs.has_value());
    device.setGpuMillisecondsPerPoll(3.5);
    const auto frame = target.frame();
    REQUIRE(frame.ok());
    REQUIRE(frame.value().gpuMs.has_value());
    CHECK(*frame.value().gpuMs == 3.5);
}
