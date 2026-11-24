// benchmark_job.hpp: Ein Benchmark-Lauf auf der GPU, mit Fortschritt auf der Konsole.
//
// Warum es diese Datei gibt: Das Anlegen des Geräts, der Plan, die Sensoren und die Fortschrittszeilen waren Teil von
// `whitehole_cli --benchmark`. Das eigene Programm `whitehole_benchmark` braucht dasselbe; statt es zu kopieren, steht es
// hier einmal. Der Messkern selbst (Stufen, Abbruchregeln, Bericht) liegt weiter Qt-frei in wh_runtime.
#pragma once

#include <atomic>
#include <string_view>

#include "benchmark_files.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/benchmark_plan.hpp"

namespace whitehole::benchapp {

struct BenchmarkJob {
    runtime::BenchmarkSeries series{runtime::BenchmarkSeries::Both};
    runtime::BenchmarkMode mode{runtime::BenchmarkMode::Full};
    bool software{false};  // Software-Adapter statt der Grafikkarte
};

/// Misst und meldet den Fortschritt auf std::cerr. `cancel` bricht ab (der Bericht enthält dann, was fertig wurde).
/// Fehler: GpuError ohne Adapter, oder wenn das Programm ohne GPU-Backend gebaut wurde (-DWHITEHOLEFX_WEBGPU=ON).
[[nodiscard]] Result<BenchmarkResult> runBenchmarkJob(const BenchmarkJob& job, const nodes::BuiltinLibrary& library,
                                                      const std::atomic<bool>& cancel, std::string_view version,
                                                      std::string_view buildId);

}  // namespace whitehole::benchapp
