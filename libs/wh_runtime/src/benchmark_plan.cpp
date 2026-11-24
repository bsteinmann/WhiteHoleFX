// benchmark_plan.cpp: Stufenplan und Statistik des Benchmarks (siehe benchmark_plan.hpp).
#include "whitehole/runtime/benchmark_plan.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace whitehole::runtime {

namespace {

constexpr std::array<BenchmarkScene, 4> kScenes{BenchmarkScene::Plasma, BenchmarkScene::Particles, BenchmarkScene::Maze,
                                                BenchmarkScene::Layers};
struct Resolution {
    std::uint32_t width;
    std::uint32_t height;
};
constexpr std::array<Resolution, 3> kResolutions{{{480, 270}, {960, 540}, {1920, 1080}}};
constexpr std::array<std::uint64_t, 4> kLedCounts{1'000, 10'000, 100'000, 1'000'000};

BenchmarkStage projection(Resolution r, BenchmarkScene scene) {
    return BenchmarkStage{BenchmarkKind::Projection, scene, r.width, r.height, 0};
}
BenchmarkStage leds(std::uint64_t count, BenchmarkScene scene) {
    return BenchmarkStage{BenchmarkKind::Leds, scene, 0, 0, count};
}

double median(std::vector<double>& v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return n % 2 == 1 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

}  // namespace

std::vector<BenchmarkStage> benchmarkPlan(BenchmarkSeries series, BenchmarkMode mode) {
    std::vector<BenchmarkStage> plan;
    const bool withProjection = series != BenchmarkSeries::Leds;
    const bool withLeds = series != BenchmarkSeries::Projection;
    if (mode == BenchmarkMode::Quick) {
        constexpr auto scene = BenchmarkScene::Layers;
        if (withProjection && withLeds) {
            plan.push_back(projection(kResolutions[1], scene));
            plan.push_back(leds(kLedCounts[1], scene));
            plan.push_back(leds(kLedCounts[2], scene));
        } else if (withProjection) {
            for (const Resolution& r : kResolutions) plan.push_back(projection(r, scene));
        } else {
            for (std::size_t i = 0; i < 3; ++i) plan.push_back(leds(kLedCounts[i], scene));
        }
        return plan;
    }
    if (withProjection) {
        for (const Resolution& r : kResolutions) {
            for (const BenchmarkScene scene : kScenes) plan.push_back(projection(r, scene));
        }
    }
    if (withLeds) {
        for (const std::uint64_t count : kLedCounts) {
            for (const BenchmarkScene scene : kScenes) plan.push_back(leds(count, scene));
        }
    }
    return plan;
}

const char* benchmarkSceneId(BenchmarkScene scene) noexcept {
    switch (scene) {
        case BenchmarkScene::Plasma: return "plasma";
        case BenchmarkScene::Particles: return "particles";
        case BenchmarkScene::Maze: return "maze";
        case BenchmarkScene::Layers: return "layers";
    }
    return "?";
}

std::string benchmarkStageLabel(const BenchmarkStage& stage) {
    if (stage.kind == BenchmarkKind::Projection) {
        return std::to_string(stage.width) + "x" + std::to_string(stage.height);
    }
    return std::to_string(stage.leds) + " LEDs";
}

BenchmarkStats summarizeFrames(std::span<const BenchmarkFrame> frames) {
    BenchmarkStats stats;
    stats.frames = frames.size();
    if (frames.empty()) {
        return stats;
    }
    std::vector<double> total;
    std::vector<double> cpu;
    std::vector<double> gpu;
    total.reserve(frames.size());
    cpu.reserve(frames.size());
    bool allGpu = true;
    for (const BenchmarkFrame& f : frames) {
        total.push_back(f.totalMs);
        cpu.push_back(f.cpuMs);
        if (f.gpuMs) {
            gpu.push_back(*f.gpuMs);
        } else {
            allGpu = false;
        }
    }
    stats.medianMs = median(total);  // sortiert `total`
    stats.maxMs = total.back();
    // 95. Perzentil nach dem Rangverfahren: kleinster Wert, unter dem mindestens 95 % liegen.
    const auto rank = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(total.size())));
    stats.p95Ms = total[std::max<std::size_t>(rank, 1) - 1];
    stats.cpuMedianMs = median(cpu);
    if (allGpu) {
        stats.gpuMedianMs = median(gpu);
    }
    return stats;
}

}  // namespace whitehole::runtime
