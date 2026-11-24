// cli_benchmark.cpp: Benchmark über die Kommandozeile (siehe cli_benchmark.hpp).
#include "cli_benchmark.hpp"

#include <iostream>

#include "benchmark_files.hpp"
#include "benchmark_job.hpp"
#include "whitehole/runtime/benchmark_report.hpp"

#ifndef WH_VERSION
#define WH_VERSION "unbekannt"
#endif
#ifndef WH_BUILD_ID
#define WH_BUILD_ID "unbekannt"
#endif

namespace whitehole::cli {

int runBenchmarkCommand(const Options& options, const nodes::BuiltinLibrary& library,
                        const std::atomic<bool>& cancel) {
    using runtime::BenchmarkSeries;
    benchapp::BenchmarkJob job;
    job.series = options.benchmark == "projection" ? BenchmarkSeries::Projection
                 : options.benchmark == "leds"     ? BenchmarkSeries::Leds
                                                   : BenchmarkSeries::Both;
    job.mode = options.benchmarkQuick ? runtime::BenchmarkMode::Quick : runtime::BenchmarkMode::Full;
    job.software = options.software;

    const auto result = benchapp::runBenchmarkJob(job, library, cancel, WH_VERSION, WH_BUILD_ID);
    if (!result.ok()) {
        std::cerr << "Fehler: " << result.error().message << "\n";
        return 2;
    }
    std::cout << runtime::benchmarkMarkdown(result.value().run, result.value().info, result.value().limits);
    if (!options.benchmarkOut.empty()) {
        if (const Result<void> saved = benchapp::writeBenchmarkFile(result.value(), options.benchmarkOut); !saved.ok()) {
            std::cerr << "Fehler: " << saved.error().message << "\n";
            return 2;
        }
        std::cerr << "Ergebnis gespeichert: " << options.benchmarkOut << "\n";
    }
    return result.value().run.aborted ? 1 : 0;
}

}  // namespace whitehole::cli
