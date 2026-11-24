// benchmark_args.hpp: Kommandozeile von whitehole_benchmark.
//
// Warum es diese Datei gibt: Das Programm ist für Menschen gedacht, die es per Doppelklick starten; ohne Argumente muss
// ein voller Lauf beider Reihen herauskommen. Die Optionen sind nur für Fortgeschrittene (Schnelltest, eine Reihe,
// Software-Adapter, Ausgabeordner). Das Auswerten steht getrennt von main.cpp, damit es ohne GPU getestet werden kann.
#pragma once

#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/runtime/benchmark_plan.hpp"

namespace whitehole::benchapp {

struct BenchmarkArgs {
    runtime::BenchmarkSeries series{runtime::BenchmarkSeries::Both};
    bool quick{false};     // --quick: Schnelltest statt vollem Lauf
    bool software{false};  // --software: Software-Adapter statt der Grafikkarte
    bool noWait{false};    // --no-wait: am Ende nicht auf Enter warten
    bool help{false};
    std::string outDir;  // --out-dir <ordner>; leer = aktueller Ordner
};

/// Fehler: InvalidArgument mit einer Meldung, die auf die Hilfe verweist.
[[nodiscard]] Result<BenchmarkArgs> parseBenchmarkArgs(const std::vector<std::string>& args);

[[nodiscard]] const char* benchmarkUsageText();

}  // namespace whitehole::benchapp
