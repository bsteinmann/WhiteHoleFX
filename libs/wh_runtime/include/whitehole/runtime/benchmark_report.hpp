// benchmark_report.hpp: Ergebnis eines Benchmark-Laufs als Tabelle (CSV, Markdown) mit Empfehlung.
//
// Warum es diese Datei gibt (Entwurf docs/entwurf-benchmark.md, Schritt 1): Ergebnisse sollen sich in Doku oder Issues
// einfügen lassen (Community-Liste „welche Karte schafft was“). Darum stehen Gerät, Treiber, Programmversion und Commit
// in jedem Export, und die Spalten sind sprachunabhängig (Kennzeichen statt übersetzter Texte), damit Tabellen
// verschiedener Nutzer vergleichbar bleiben.
#pragma once

#include <optional>
#include <string>

#include "whitehole/runtime/benchmark_runner.hpp"

namespace whitehole::runtime {

struct BenchmarkSystemInfo {
    std::string device;   // Adaptername, z. B. "NVIDIA GeForce GTX 1060"
    std::string backend;  // z. B. "Vulkan"
    std::string driver;   // Treiberversion, leer wenn unbekannt
    std::string version;  // Programmversion
    std::string build;    // Commit und CI-Lauf
};

/// „Stabil“: gemessen, nicht am Limit, 95. Perzentil im Budget (30 fps).
[[nodiscard]] bool benchmarkStageStable(const BenchmarkStageResult& result, const BenchmarkLimits& limits = {});

/// Größte Stufe einer Reihe, bei der jede gemessene Szene stabil ist; nullopt, wenn schon die kleinste nicht reicht oder
/// die Reihe nicht lief. Ergebnis ist die Stufe der ersten Szene dieser Größe (für die Bezeichnung).
[[nodiscard]] std::optional<BenchmarkStage> benchmarkRecommendation(const BenchmarkRun& run, BenchmarkKind kind,
                                                                    const BenchmarkLimits& limits = {});

/// Kennzeichen des Ergebnisses: "ok", "over", "skipped", "not-run", "failed".
[[nodiscard]] const char* benchmarkOutcomeId(BenchmarkOutcome outcome) noexcept;

/// Kopfzeilen mit `#` (System), dann
/// `kind,stage,scene,points,outcome,frames,median_ms,p95_ms,max_ms,cpu_ms,gpu_ms,at_limit,temp_c,clock_mhz,note`.
[[nodiscard]] std::string benchmarkCsv(const BenchmarkRun& run, const BenchmarkSystemInfo& info);
/// Systemangaben, Tabelle, Empfehlung je Reihe und ggf. Abbruchgrund.
[[nodiscard]] std::string benchmarkMarkdown(const BenchmarkRun& run, const BenchmarkSystemInfo& info,
                                            const BenchmarkLimits& limits = {});

}  // namespace whitehole::runtime
