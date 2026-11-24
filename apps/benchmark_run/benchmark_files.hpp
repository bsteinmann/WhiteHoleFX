// benchmark_files.hpp: Ergebnis eines Benchmark-Laufs und seine Ausgabe in Dateien.
//
// Warum es diese Datei gibt: `whitehole_cli --benchmark` und das eigene Programm `whitehole_benchmark` schreiben
// dieselben Berichte (Markdown und CSV) und brauchen dieselben Dateinamen. Der Teil kommt ohne GPU aus und ist deshalb
// von der Messung (benchmark_job.hpp) getrennt: Die Tests prüfen ihn auch dort, wo kein Grafik-Backend gebaut wird.
#pragma once

#include <ctime>
#include <string>

#include "whitehole/core/result.hpp"
#include "whitehole/runtime/benchmark_report.hpp"
#include "whitehole/runtime/benchmark_runner.hpp"

namespace whitehole::benchapp {

/// Alles, was ein Bericht braucht: die Messung, das System und die Grenzen, mit denen gemessen wurde.
struct BenchmarkResult {
    runtime::BenchmarkRun run;
    runtime::BenchmarkSystemInfo info;
    runtime::BenchmarkLimits limits;
};

/// Endet der Pfad auf ".csv"? Sonst gilt Markdown.
[[nodiscard]] bool hasCsvExtension(const std::string& path) noexcept;

/// Schreibt den Bericht als CSV (Endung ".csv") oder Markdown. Fehler: IoError mit dem Pfad.
[[nodiscard]] Result<void> writeBenchmarkFile(const BenchmarkResult& result, const std::string& path);

/// Dateiname ohne Endung, z. B. "whitehole-benchmark-20261124-1841" für den 24.11.2026 um 18:41 Uhr.
[[nodiscard]] std::string benchmarkFileStem(const std::tm& local);

}  // namespace whitehole::benchapp
