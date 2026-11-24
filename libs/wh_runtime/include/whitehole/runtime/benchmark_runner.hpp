// benchmark_runner.hpp: führt einen Benchmark-Plan aus und wendet die Abbruchregeln an.
//
// Warum es diese Datei gibt (Entwurf docs/entwurf-benchmark.md, Schritt 1): Die Regeln, wann eine Stufe als „über
// Grenze“ gilt, wann größere Stufen übersprungen werden und wann der ganze Lauf abbricht, sollen ohne GPU prüfbar sein.
// Darum misst der Runner nicht selbst, sondern fragt ein `IBenchmarkTarget` nach Frames; die Uhr ist ebenfalls
// austauschbar. Echte Ziele (GPU-Session) und Oberflächen (CLI, Studio) bauen darauf auf.
//
// Ablauf je Stufe: prepare (Szene und Größe aufbauen), Aufwärmen (Zeit `warmupSeconds`, Frames verworfen, auch weil
// Shader beim ersten Frame übersetzt werden), Messen (Zeit `measureSeconds`). Abbruchregeln:
// - Ein einzelner Frame über `frameAbortMs`: ganzer Lauf bricht ab (unter Windows setzt der Treiber die GPU nach rund
//   2 s zurück, TDR). Geprüft wird die GPU-Zeit, ohne sie die Gesamtzeit; der erste Frame einer Stufe zählt ohne
//   GPU-Zeit nicht (Shader-Übersetzung). Die Grenze greift erst nach dem Frame; darum zusätzlich die Hochrechnung.
// - Median über `overFactor` × Budget: Stufe „über Grenze“, größere Stufen derselben Reihe und Szene werden übersprungen.
// - Hochrechnung: Würde eine Stufe nach dem Median der vorigen Stufe derselben Reihe und Szene (linear in der Zahl der
//   Abtastpunkte) über `frameAbortMs` liegen, wird sie übersprungen, ohne einen Frame zu rendern.
// - 95. Perzentil über `spikeFactor` × Median: Stufe bleibt gültig, ist aber als „am Limit“ markiert.
// - Fehler des Ziels (GPU-Fehler, verlorenes Gerät, Speichermangel): sofortiger Abbruch mit Angabe der Stufe.
// - Abbrechen von außen (`cancel`): Abbruch nach dem laufenden Frame.
#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/runtime/benchmark_plan.hpp"
#include "whitehole/runtime/benchmark_sensors.hpp"

namespace whitehole::runtime {

class IBenchmarkTarget {
public:
    virtual ~IBenchmarkTarget() = default;
    /// Baut Szene und Größe der Stufe auf. Fehler: beendet den Lauf.
    virtual Result<void> prepare(const BenchmarkStage& stage) = 0;
    /// Rendert genau einen Frame und meldet seine Zeiten. Fehler: beendet den Lauf.
    virtual Result<BenchmarkFrame> frame() = 0;
};

struct BenchmarkLimits {
    double budgetMs{1000.0 / 30.0};  // 30 fps
    double overFactor{2.0};
    double spikeFactor{3.0};
    double frameAbortMs{500.0};
    double warmupSeconds{2.0};
    double measureSeconds{10.0};
    std::size_t minFrames{5};  // so viele Frames mindestens messen, auch wenn das Messfenster dafür länger wird
};

enum class BenchmarkOutcome {
    Ok,           // gemessen, im Rahmen
    OverBudget,   // gemessen, Median über overFactor × Budget
    Skipped,      // nicht gemessen: kleinere Stufe war über Grenze oder Hochrechnung zu hoch
    NotRun,       // nicht gemessen: Lauf vorher abgebrochen
    Failed,       // Lauf an dieser Stufe abgebrochen (Fehler, Einzelframe zu lang, Abbrechen)
};

struct BenchmarkStageResult {
    BenchmarkStage stage;
    BenchmarkOutcome outcome{BenchmarkOutcome::NotRun};
    BenchmarkStats stats;
    bool atLimit{false};   // 95. Perzentil stark ausreißend
    std::string note;      // Grund bei Skipped/Failed
    std::optional<unsigned> maxTemperatureC;  // höchste GPU-Temperatur während der Messung (Sensoren, sonst leer)
    std::optional<unsigned> minClockMHz;      // niedrigster Grafiktakt während der Messung (Einbruch durch Hitze)
};

struct BenchmarkRun {
    std::vector<BenchmarkStageResult> stages;  // eine Zeile je Stufe des Plans, in Planreihenfolge
    bool aborted{false};
    std::string abortReason;  // mit Angabe der auslösenden Stufe
};

struct BenchmarkHooks {
    std::function<double()> nowSeconds;                         // leer: steady_clock
    std::function<void(std::size_t index, std::size_t count)> stageStarted;  // vor jeder Stufe (auch übersprungenen)
    const std::atomic<bool>* cancel{nullptr};                   // true: nach dem laufenden Frame abbrechen
    IGpuSensors* sensors{nullptr};                              // gelesen zu Beginn der Messung und dann jede Sekunde
};

[[nodiscard]] BenchmarkRun runBenchmark(const std::vector<BenchmarkStage>& plan, IBenchmarkTarget& target,
                                        const BenchmarkLimits& limits = {}, const BenchmarkHooks& hooks = {});

}  // namespace whitehole::runtime
