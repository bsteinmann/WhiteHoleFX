// benchmark_report_test.cpp: CSV, Markdown und Empfehlung des Benchmarks.
//
// Warum es die Datei gibt: Exporte landen in Doku und Issues anderer Nutzer; Spalten, Kennzeichen und das Maskieren von
// Sonderzeichen müssen stabil bleiben, und die Empfehlung darf keine Stufe nennen, die nicht verlässlich lief.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "whitehole/runtime/benchmark_report.hpp"

using namespace whitehole::runtime;
using Catch::Matchers::ContainsSubstring;

namespace {

BenchmarkStageResult row(std::uint64_t leds, BenchmarkScene scene, BenchmarkOutcome outcome, double median,
                         double p95, bool atLimit = false) {
    BenchmarkStageResult r;
    r.stage = BenchmarkStage{BenchmarkKind::Leds, scene, 0, 0, leds};
    r.outcome = outcome;
    r.stats.frames = outcome == BenchmarkOutcome::Ok || outcome == BenchmarkOutcome::OverBudget ? 100 : 0;
    r.stats.medianMs = median;
    r.stats.p95Ms = p95;
    r.stats.maxMs = p95 + 1.0;
    r.stats.cpuMedianMs = median / 4.0;
    r.atLimit = atLimit;
    return r;
}

BenchmarkSystemInfo info() {
    return BenchmarkSystemInfo{"NVIDIA GeForce GTX 1060", "Vulkan", "566.36", "1.1.0", "commit-a8f82c684b, lauf 1"};
}

}  // namespace

TEST_CASE("Benchmark-Empfehlung: größte Größe, bei der jede Szene stabil ist", "[benchmark]") {
    BenchmarkRun run;
    run.stages = {row(1'000, BenchmarkScene::Plasma, BenchmarkOutcome::Ok, 2, 3),
                  row(1'000, BenchmarkScene::Maze, BenchmarkOutcome::Ok, 2, 3),
                  row(10'000, BenchmarkScene::Plasma, BenchmarkOutcome::Ok, 10, 20),
                  row(10'000, BenchmarkScene::Maze, BenchmarkOutcome::Ok, 12, 25),
                  row(100'000, BenchmarkScene::Plasma, BenchmarkOutcome::Ok, 20, 30),
                  row(100'000, BenchmarkScene::Maze, BenchmarkOutcome::Ok, 30, 45),  // P95 über 33 ms
                  row(1'000'000, BenchmarkScene::Plasma, BenchmarkOutcome::Ok, 25, 30)};
    const auto best = benchmarkRecommendation(run, BenchmarkKind::Leds);
    REQUIRE(best.has_value());
    CHECK(best->leds == 10'000);  // 1 Mio. zählt nicht, weil schon 100 000 nicht verlässlich lief
    CHECK_FALSE(benchmarkRecommendation(run, BenchmarkKind::Projection).has_value());

    run.stages[1].atLimit = true;
    CHECK_FALSE(benchmarkRecommendation(run, BenchmarkKind::Leds).has_value());
}

TEST_CASE("Benchmark-Export: CSV mit Systemkopf und maskierten Feldern", "[benchmark]") {
    BenchmarkRun run;
    run.stages = {row(1'000, BenchmarkScene::Plasma, BenchmarkOutcome::Ok, 2.5, 3.25),
                  row(10'000, BenchmarkScene::Plasma, BenchmarkOutcome::Skipped, 0, 0)};
    run.stages[0].stats.gpuMedianMs = 1.5;
    run.stages[1].note = "kleinere Stufe über Grenze, \"zu langsam\"";
    const std::string csv = benchmarkCsv(run, info());
    CHECK_THAT(csv, ContainsSubstring("# device,NVIDIA GeForce GTX 1060\n"));
    CHECK_THAT(csv, ContainsSubstring("# build,\"commit-a8f82c684b, lauf 1\"\n"));
    CHECK_THAT(csv, ContainsSubstring("kind,stage,scene,points,outcome,frames,median_ms,p95_ms,max_ms,cpu_ms,gpu_ms,at_limit,note\n"));
    CHECK_THAT(csv, ContainsSubstring("leds,1000 LEDs,plasma,1000,ok,100,2.50,3.25,4.25,0.62,1.50,0,\n"));
    CHECK_THAT(csv, ContainsSubstring("leds,10000 LEDs,plasma,10000,skipped,0,,,,,,0,\"kleinere Stufe über Grenze, \"\"zu langsam\"\"\"\n"));
}

TEST_CASE("Benchmark-Export: Markdown mit Tabelle, Empfehlung und Abbruchgrund", "[benchmark]") {
    BenchmarkRun run;
    run.stages = {row(1'000, BenchmarkScene::Layers, BenchmarkOutcome::Ok, 2, 3, true),
                  row(10'000, BenchmarkScene::Layers, BenchmarkOutcome::Failed, 0, 0)};
    run.stages[1].note = "Gerät | verloren";
    run.aborted = true;
    run.abortReason = "10000 LEDs layers: Gerät verloren";
    const std::string md = benchmarkMarkdown(run, info());
    CHECK_THAT(md, ContainsSubstring("**WhiteHoleFX-Benchmark** 1.1.0"));
    CHECK_THAT(md, ContainsSubstring("- Gerät: NVIDIA GeForce GTX 1060 [Vulkan]\n- Treiber: 566.36"));
    CHECK_THAT(md, ContainsSubstring("| leds | 1000 LEDs | layers | ok | 2.00 | 3.00 | 0.50 |  | am Limit |\n"));
    CHECK_THAT(md, ContainsSubstring("| leds | 10000 LEDs | layers | failed |  |  |  |  | Gerät \\| verloren |\n"));
    CHECK_THAT(md, ContainsSubstring("- Stabil bei 30 fps (leds): keine Stufe\n"));
    CHECK_THAT(md, ContainsSubstring("- Abgebrochen: 10000 LEDs layers: Gerät verloren\n"));
    CHECK_THAT(md, !ContainsSubstring("(projection)"));  // Reihe lief nicht
}
