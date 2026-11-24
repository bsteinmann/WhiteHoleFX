// benchmark_runner.cpp: Ablauf je Stufe und Abbruchregeln (siehe benchmark_runner.hpp).
#include "whitehole/runtime/benchmark_runner.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <tuple>
#include <utility>

namespace whitehole::runtime {

namespace {

double steadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

using SeriesKey = std::tuple<BenchmarkKind, BenchmarkScene>;

struct Previous {
    std::uint64_t points{0};
    double medianMs{0.0};
    bool overBudget{false};
};

bool cancelled(const BenchmarkHooks& hooks) {
    return hooks.cancel != nullptr && hooks.cancel->load(std::memory_order_relaxed);
}

std::string stageName(const BenchmarkStage& stage) {
    return benchmarkStageLabel(stage) + " " + benchmarkSceneId(stage.scene);
}

}  // namespace

BenchmarkRun runBenchmark(const std::vector<BenchmarkStage>& plan, IBenchmarkTarget& target,
                          const BenchmarkLimits& limits, const BenchmarkHooks& hooks) {
    const std::function<double()> now = hooks.nowSeconds ? hooks.nowSeconds : std::function<double()>{steadySeconds};
    BenchmarkRun run;
    run.stages.reserve(plan.size());
    for (const BenchmarkStage& stage : plan) {
        BenchmarkStageResult row;
        row.stage = stage;
        run.stages.push_back(std::move(row));
    }
    std::map<SeriesKey, Previous> previous;
    const auto fail = [&](BenchmarkStageResult& result, std::string reason) {
        result.outcome = BenchmarkOutcome::Failed;
        result.note = reason;
        run.aborted = true;
        run.abortReason = stageName(result.stage) + ": " + std::move(reason);
    };

    for (std::size_t i = 0; i < plan.size() && !run.aborted; ++i) {
        BenchmarkStageResult& result = run.stages[i];
        const BenchmarkStage& stage = plan[i];
        if (hooks.stageStarted) {
            hooks.stageStarted(i, plan.size());
        }
        const SeriesKey key{stage.kind, stage.scene};
        if (const auto it = previous.find(key); it != previous.end()) {
            const Previous& p = it->second;
            if (p.overBudget) {
                result.outcome = BenchmarkOutcome::Skipped;
                result.note = "kleinere Stufe über Grenze";
                continue;
            }
            const double estimate = p.points == 0 ? 0.0
                                                  : p.medianMs * static_cast<double>(stage.points()) /
                                                        static_cast<double>(p.points);
            if (estimate > limits.frameAbortMs) {
                result.outcome = BenchmarkOutcome::Skipped;
                result.note = "hochgerechnet " + std::to_string(static_cast<long>(estimate)) + " ms je Frame";
                previous[key] = Previous{stage.points(), estimate, true};
                continue;
            }
        }
        if (cancelled(hooks)) {
            fail(result, "abgebrochen");
            break;
        }
        if (auto prepared = target.prepare(stage); !prepared.ok()) {
            fail(result, prepared.error().message);
            break;
        }
        std::vector<BenchmarkFrame> frames;
        std::size_t frameIndex = 0;
        const double start = now();
        double nextSensorRead = limits.warmupSeconds;
        const auto sample = [&] {
            const GpuSensorReading reading = hooks.sensors->read();
            if (reading.temperatureC) {
                result.maxTemperatureC = std::max(result.maxTemperatureC.value_or(0U), *reading.temperatureC);
            }
            if (reading.graphicsClockMHz) {
                result.minClockMHz = std::min(result.minClockMHz.value_or(~0U), *reading.graphicsClockMHz);
            }
        };
        for (;;) {
            if (cancelled(hooks)) {
                fail(result, "abgebrochen");
                break;
            }
            const double frameStart = now() - start;
            auto f = target.frame();
            if (!f.ok()) {
                fail(result, f.error().message);
                break;
            }
            // TDR betrifft die Laufzeit auf der GPU; ohne GPU-Zeit zählt die Gesamtzeit, außer beim ersten Frame der
            // Stufe (Shader-Übersetzung auf der CPU, kein Risiko für den Treiber).
            const BenchmarkFrame& got = f.value();
            const double checkMs = got.gpuMs ? *got.gpuMs : got.totalMs;
            if ((got.gpuMs || frameIndex > 0) && checkMs > limits.frameAbortMs) {
                fail(result, "Einzelframe " + std::to_string(static_cast<long>(checkMs)) + " ms");
                break;
            }
            ++frameIndex;
            const double elapsed = now() - start;
            // Gemessen wird nur, was nach dem Aufwärmen begonnen hat; der erste Frame zählt nie (Shader-Übersetzung).
            if (frameStart >= limits.warmupSeconds && frameIndex > 1) {
                frames.push_back(got);
            }
            if (hooks.sensors != nullptr && elapsed >= nextSensorRead) {
                sample();
                nextSensorRead = elapsed + 1.0;
            }
            if (elapsed >= limits.warmupSeconds + limits.measureSeconds && frames.size() >= limits.minFrames) {
                break;
            }
        }
        if (run.aborted) {
            break;
        }
        result.stats = summarizeFrames(frames);
        const bool over = result.stats.medianMs > limits.overFactor * limits.budgetMs;
        result.outcome = over ? BenchmarkOutcome::OverBudget : BenchmarkOutcome::Ok;
        result.atLimit = result.stats.frames > 0 && result.stats.p95Ms > limits.spikeFactor * result.stats.medianMs;
        previous[key] = Previous{stage.points(), result.stats.medianMs, over};
    }
    return run;
}

}  // namespace whitehole::runtime
