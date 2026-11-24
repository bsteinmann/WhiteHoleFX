// cli_benchmark.cpp: Benchmark über die Kommandozeile (siehe cli_benchmark.hpp).
#include "cli_benchmark.hpp"

#include <fstream>
#include <iostream>

#include "whitehole/runtime/benchmark_report.hpp"

#ifdef WH_CLI_HAVE_GPU
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/runtime/benchmark_gpu_target.hpp"
#endif

#ifndef WH_VERSION
#define WH_VERSION "unbekannt"
#endif
#ifndef WH_BUILD_ID
#define WH_BUILD_ID "unbekannt"
#endif

namespace whitehole::cli {

#ifdef WH_CLI_HAVE_GPU
int runBenchmarkCommand(const Options& options, const nodes::BuiltinLibrary& library,
                        const std::atomic<bool>& cancel) {
    using namespace runtime;
    gpu::WebGpuOptions gpuOptions;
    gpuOptions.forceFallbackAdapter = options.software;
    gpuOptions.label = "WhiteHoleFX Benchmark";
    gpuOptions.timestamps = true;
    auto device = gpu::WebGpuDevice::create(gpuOptions);
    if (!device.ok()) {
        std::cerr << "Fehler: " << device.error().message << "\n";
        return 2;
    }
    const BenchmarkSeries series = options.benchmark == "projection" ? BenchmarkSeries::Projection
                                   : options.benchmark == "leds"     ? BenchmarkSeries::Leds
                                                                     : BenchmarkSeries::Both;
    const auto plan = benchmarkPlan(series, options.benchmarkQuick ? BenchmarkMode::Quick : BenchmarkMode::Full);
    std::cerr << "GPU-Adapter: " << device.value()->adapterName() << (options.software ? " (Software)" : "")
              << (device.value()->measuresGpuTime() ? "" : ", ohne GPU-Zeitstempel") << "\n"
              << "Benchmark: " << plan.size() << " Stufen, je etwa 12 s (Strg+C bricht ab).\n";

    GpuBenchmarkTarget target{*device.value(), library};
    BenchmarkHooks hooks;
    hooks.cancel = &cancel;
    hooks.stageStarted = [&plan](std::size_t i, std::size_t count) {
        std::cerr << "  Stufe " << (i + 1) << "/" << count << ": " << benchmarkStageLabel(plan[i]) << " "
                  << benchmarkSceneId(plan[i].scene) << "\n";
    };
    const BenchmarkLimits limits;
    const BenchmarkRun run = runBenchmark(plan, target, limits, hooks);

    const BenchmarkSystemInfo info{device.value()->deviceName(), device.value()->backendName(),
                                   device.value()->driverInfo(), WH_VERSION, WH_BUILD_ID};
    std::cout << benchmarkMarkdown(run, info, limits);
    if (!options.benchmarkOut.empty()) {
        const bool csv = options.benchmarkOut.size() > 4 &&
                         options.benchmarkOut.compare(options.benchmarkOut.size() - 4, 4, ".csv") == 0;
        std::ofstream file{options.benchmarkOut, std::ios::binary};
        file << (csv ? benchmarkCsv(run, info) : benchmarkMarkdown(run, info, limits));
        if (!file) {
            std::cerr << "Fehler: " << options.benchmarkOut << " ließ sich nicht schreiben.\n";
            return 2;
        }
        std::cerr << "Ergebnis gespeichert: " << options.benchmarkOut << "\n";
    }
    return run.aborted ? 1 : 0;
}
#else
int runBenchmarkCommand(const Options&, const nodes::BuiltinLibrary&, const std::atomic<bool>&) {
    std::cerr << "Dieses Programm wurde ohne GPU-Backend gebaut; der Benchmark braucht -DWHITEHOLEFX_WEBGPU=ON.\n";
    return 2;
}
#endif

}  // namespace whitehole::cli
