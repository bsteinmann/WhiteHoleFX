// benchmark_test.cpp: Stufenplan, Statistik und Abbruchregeln des Benchmarks, ohne GPU.
//
// Warum es die Datei gibt: Die Regeln (Aufwärmen verwerfen, „über Grenze“, Überspringen, Hochrechnung, Einzelframe,
// Abbrechen, Fehler) entscheiden, ob ein Lauf die GPU in einen Treiber-Reset treibt. Sie laufen hier gegen ein Fake-Ziel
// mit fester Zeit je Abtastpunkt und eine Fake-Uhr, die jeder Frame weiterstellt.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <functional>
#include <vector>

#include "whitehole/runtime/benchmark_runner.hpp"

using namespace whitehole;
using namespace whitehole::runtime;

namespace {

/// Ein Frame kostet `msPerMillionPoints` je Million Abtastpunkte; die Uhr läuft um genau diese Zeit weiter.
struct FakeTarget final : IBenchmarkTarget {
    double msPerMillionPoints{1.0};
    double firstFrameMs{0.0};       // > 0: erster Frame jeder Stufe dauert so lange (Shader übersetzen)
    std::optional<double> gpuMs;    // gesetzt: jeder Frame meldet diese GPU-Zeit
    std::optional<std::size_t> failPrepareAt;
    std::optional<std::size_t> failFrameAfter;  // nach so vielen Frames insgesamt
    std::function<void(std::size_t)> onFrame;   // z. B. Abbrechen auslösen
    double clock{0.0};
    std::size_t prepared{0};
    std::size_t frames{0};
    std::size_t framesInStage{0};
    BenchmarkStage current;
    std::vector<BenchmarkStage> preparedStages;

    Result<void> prepare(const BenchmarkStage& stage) override {
        if (failPrepareAt && prepared == *failPrepareAt) {
            return Error{ErrorCode::GpuError, "Speicher voll"};
        }
        ++prepared;
        current = stage;
        framesInStage = 0;
        preparedStages.push_back(stage);
        return {};
    }
    Result<BenchmarkFrame> frame() override {
        if (failFrameAfter && frames >= *failFrameAfter) {
            return Error{ErrorCode::GpuError, "Gerät verloren"};
        }
        double t = msPerMillionPoints * static_cast<double>(current.points()) / 1e6;
        if (framesInStage == 0 && firstFrameMs > 0.0) {
            t = firstFrameMs;
        }
        ++frames;
        ++framesInStage;
        clock += t / 1000.0;
        if (onFrame) onFrame(frames);
        return BenchmarkFrame{t * 0.25, gpuMs, t};
    }
};

BenchmarkHooks clockOf(FakeTarget& target) {
    BenchmarkHooks hooks;
    hooks.nowSeconds = [&target] { return target.clock; };
    return hooks;
}

BenchmarkLimits shortLimits() {
    BenchmarkLimits limits;
    limits.warmupSeconds = 0.1;
    limits.measureSeconds = 0.5;
    return limits;
}

}  // namespace

TEST_CASE("Benchmark-Plan: Reihen, Reihenfolge und Schnelltest", "[benchmark]") {
    const auto full = benchmarkPlan(BenchmarkSeries::Both, BenchmarkMode::Full);
    REQUIRE(full.size() == 28);  // 3 × 4 Projektion + 4 × 4 LEDs, kein Kreuzprodukt
    CHECK(benchmarkPlan(BenchmarkSeries::Projection, BenchmarkMode::Full).size() == 12);
    CHECK(benchmarkPlan(BenchmarkSeries::Leds, BenchmarkMode::Full).size() == 16);
    CHECK(full.front() == BenchmarkStage{BenchmarkKind::Projection, BenchmarkScene::Plasma, 480, 270, 0});
    CHECK(full.back() == BenchmarkStage{BenchmarkKind::Leds, BenchmarkScene::Layers, 0, 0, 1'000'000});
    for (std::size_t i = 1; i < full.size(); ++i) {
        if (full[i].kind == full[i - 1].kind) {
            CHECK(full[i].points() >= full[i - 1].points());  // je Reihe vom kleinsten zum größten Aufwand
        }
    }
    const auto quick = benchmarkPlan(BenchmarkSeries::Both, BenchmarkMode::Quick);
    REQUIRE(quick.size() == 3);
    CHECK(quick[0].kind == BenchmarkKind::Projection);
    CHECK(quick[2].leds == 100'000);
    CHECK(benchmarkPlan(BenchmarkSeries::Leds, BenchmarkMode::Quick).size() == 3);
    CHECK(benchmarkPlan(BenchmarkSeries::Projection, BenchmarkMode::Quick).back().width == 1920);
    CHECK(benchmarkStageLabel(full.front()) == "480x270");
    CHECK(benchmarkStageLabel(full.back()) == "1000000 LEDs");
}

TEST_CASE("Benchmark-Statistik: Median, 95. Perzentil, GPU nur wenn überall vorhanden", "[benchmark]") {
    std::vector<BenchmarkFrame> frames;
    for (int i = 1; i <= 20; ++i) {
        frames.push_back(BenchmarkFrame{1.0, 2.0, static_cast<double>(i)});
    }
    BenchmarkStats s = summarizeFrames(frames);
    CHECK(s.frames == 20);
    CHECK(s.medianMs == 10.5);
    CHECK(s.p95Ms == 19.0);  // Rang ceil(0.95 * 20) = 19
    CHECK(s.maxMs == 20.0);
    CHECK(s.cpuMedianMs == 1.0);
    REQUIRE(s.gpuMedianMs.has_value());
    frames[3].gpuMs.reset();
    CHECK_FALSE(summarizeFrames(frames).gpuMedianMs.has_value());
    CHECK(summarizeFrames({}).frames == 0);
}

TEST_CASE("Benchmark: Aufwärmen wird verworfen, alles im Rahmen ist ok", "[benchmark]") {
    FakeTarget target;
    target.msPerMillionPoints = 10.0;
    target.firstFrameMs = 900.0;  // Shader-Übersetzung: erster Frame zählt weder als Messung noch als Abbruch
    const std::vector<BenchmarkStage> plan{{BenchmarkKind::Leds, BenchmarkScene::Plasma, 0, 0, 1'000'000}};
    BenchmarkLimits limits = shortLimits();
    limits.measureSeconds = 1.5;  // der erste Frame allein verbraucht schon 0,9 s
    const BenchmarkRun run = runBenchmark(plan, target, limits, clockOf(target));
    REQUIRE_FALSE(run.aborted);
    REQUIRE(run.stages[0].outcome == BenchmarkOutcome::Ok);
    CHECK(run.stages[0].stats.maxMs == 10.0);  // der lange erste Frame ist nicht in der Messung
    CHECK(run.stages[0].stats.frames >= 40);
    CHECK_FALSE(run.stages[0].atLimit);
}

TEST_CASE("Benchmark: über Grenze überspringt größere Stufen derselben Szene", "[benchmark]") {
    FakeTarget target;
    target.msPerMillionPoints = 700.0;  // 100 000 LEDs = 70 ms (über 2 × 33 ms), 10 000 = 7 ms
    const BenchmarkRun run = runBenchmark(benchmarkPlan(BenchmarkSeries::Leds, BenchmarkMode::Full), target,
                                          shortLimits(), clockOf(target));
    REQUIRE_FALSE(run.aborted);
    for (const BenchmarkStageResult& r : run.stages) {
        if (r.stage.leds <= 10'000) CHECK(r.outcome == BenchmarkOutcome::Ok);
        if (r.stage.leds == 100'000) CHECK(r.outcome == BenchmarkOutcome::OverBudget);
        if (r.stage.leds == 1'000'000) {
            CHECK(r.outcome == BenchmarkOutcome::Skipped);
            CHECK(r.note == "kleinere Stufe über Grenze");
        }
    }
    for (const BenchmarkStage& s : target.preparedStages) {
        CHECK(s.leds < 1'000'000);  // die übersprungenen Stufen wurden nie aufgebaut
    }
}

TEST_CASE("Benchmark: Hochrechnung überspringt Stufen, die einen Treiber-Reset riskieren", "[benchmark]") {
    FakeTarget target;
    target.msPerMillionPoints = 600.0;  // 100 000 LEDs = 60 ms, das wäre bei 1 Mio. 600 ms
    BenchmarkLimits limits = shortLimits();
    limits.overFactor = 100.0;  // „über Grenze“ aus dem Spiel nehmen, nur die Hochrechnung prüfen
    const std::vector<BenchmarkStage> plan{{BenchmarkKind::Leds, BenchmarkScene::Maze, 0, 0, 100'000},
                                           {BenchmarkKind::Leds, BenchmarkScene::Maze, 0, 0, 1'000'000}};
    const BenchmarkRun run = runBenchmark(plan, target, limits, clockOf(target));
    CHECK_FALSE(run.aborted);
    CHECK(run.stages[0].outcome == BenchmarkOutcome::Ok);
    CHECK(run.stages[1].outcome == BenchmarkOutcome::Skipped);
    CHECK_THAT(run.stages[1].note, Catch::Matchers::StartsWith("hochgerechnet 600"));
    CHECK(target.prepared == 1);
}

TEST_CASE("Benchmark: Einzelframe über 500 ms bricht den ganzen Lauf ab", "[benchmark]") {
    FakeTarget target;
    target.msPerMillionPoints = 700.0;
    target.gpuMs = 700.0;  // mit GPU-Zeit zählt auch schon der erste Frame
    const std::vector<BenchmarkStage> plan{{BenchmarkKind::Leds, BenchmarkScene::Plasma, 0, 0, 1'000'000},
                                           {BenchmarkKind::Leds, BenchmarkScene::Particles, 0, 0, 1'000'000}};
    const BenchmarkRun run = runBenchmark(plan, target, shortLimits(), clockOf(target));
    REQUIRE(run.aborted);
    CHECK(run.stages[0].outcome == BenchmarkOutcome::Failed);
    CHECK(run.stages[1].outcome == BenchmarkOutcome::NotRun);
    CHECK(run.abortReason == "1000000 LEDs plasma: Einzelframe 700 ms");
    CHECK(target.frames == 1);
}

TEST_CASE("Benchmark: GPU-Fehler und Abbrechen beenden den Lauf mit Angabe der Stufe", "[benchmark]") {
    const auto plan = benchmarkPlan(BenchmarkSeries::Projection, BenchmarkMode::Quick);
    SECTION("Fehler beim Aufbau") {
        FakeTarget target;
        target.failPrepareAt = 1;
        const BenchmarkRun run = runBenchmark(plan, target, shortLimits(), clockOf(target));
        REQUIRE(run.aborted);
        CHECK(run.stages[0].outcome == BenchmarkOutcome::Ok);
        CHECK(run.stages[1].outcome == BenchmarkOutcome::Failed);
        CHECK(run.abortReason == "960x540 layers: Speicher voll");
    }
    SECTION("Fehler während eines Frames") {
        FakeTarget target;
        target.failFrameAfter = 3;
        const BenchmarkRun run = runBenchmark(plan, target, shortLimits(), clockOf(target));
        REQUIRE(run.aborted);
        CHECK(run.stages[0].outcome == BenchmarkOutcome::Failed);
        CHECK(run.stages[0].note == "Gerät verloren");
    }
    SECTION("Abbrechen von außen") {
        FakeTarget target;
        std::atomic<bool> cancel{false};
        target.onFrame = [&cancel](std::size_t n) { if (n == 5) cancel = true; };
        BenchmarkHooks hooks = clockOf(target);
        hooks.cancel = &cancel;
        std::vector<std::size_t> started;
        hooks.stageStarted = [&started](std::size_t i, std::size_t count) { started.push_back(i); CHECK(count == 3); };
        const BenchmarkRun run = runBenchmark(plan, target, shortLimits(), hooks);
        REQUIRE(run.aborted);
        CHECK(run.stages[0].note == "abgebrochen");
        CHECK(target.frames == 5);
        CHECK(started == std::vector<std::size_t>{0});
    }
}

TEST_CASE("Benchmark: starke Ausreißer markieren die Stufe als am Limit", "[benchmark]") {
    struct Spiky final : IBenchmarkTarget {
        double clock{0.0};
        int n{0};
        Result<void> prepare(const BenchmarkStage&) override { return {}; }
        Result<BenchmarkFrame> frame() override {
            const double t = (++n % 10 == 0) ? 40.0 : 5.0;  // jeder zehnte Frame 8-mal so lang
            clock += t / 1000.0;
            return BenchmarkFrame{1.0, std::nullopt, t};
        }
    } target;
    BenchmarkHooks hooks;
    hooks.nowSeconds = [&target] { return target.clock; };
    const BenchmarkRun run = runBenchmark({{BenchmarkKind::Projection, BenchmarkScene::Plasma, 480, 270, 0}}, target,
                                          shortLimits(), hooks);
    REQUIRE(run.stages[0].outcome == BenchmarkOutcome::Ok);
    CHECK(run.stages[0].atLimit);
}

TEST_CASE("Benchmark: auch langsame Stufen haben mindestens fünf gemessene Frames", "[benchmark]") {
    FakeTarget target;
    target.msPerMillionPoints = 400.0;  // 1 Mio. LEDs = 400 ms je Frame: das Messfenster allein brächte zwei Frames
    BenchmarkLimits limits = shortLimits();
    limits.overFactor = 100.0;
    const BenchmarkRun run = runBenchmark({{BenchmarkKind::Leds, BenchmarkScene::Plasma, 0, 0, 1'000'000}}, target, limits,
                                          clockOf(target));
    REQUIRE(run.stages[0].outcome == BenchmarkOutcome::Ok);
    CHECK(run.stages[0].stats.frames == 5);
    CHECK(run.stages[0].stats.medianMs == 400.0);
}
