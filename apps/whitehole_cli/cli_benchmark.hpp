// cli_benchmark.hpp: `whitehole_cli --benchmark` (Entwurf docs/entwurf-benchmark.md, Schritt 1).
//
// Warum es diese Datei gibt: Der Benchmark-Kern ist Qt-frei; über das CLI lässt er sich auf jedem Rechner (und lokal ohne
// Studio-Build) starten, und Nutzer anderer Karten können die Tabelle direkt in ein Issue kopieren. main.cpp bleibt so
// klein; hier stehen Gerät anlegen, Fortschritt, Ausgabe und Datei-Export.
#pragma once

#include <atomic>

#include "cli_options.hpp"
#include "whitehole/nodes/builtin_library.hpp"

namespace whitehole::cli {

/// Rückgabe: 0 = Lauf vollständig, 1 = abgebrochen (Bericht steht trotzdem da), 2 = keine GPU oder Datei nicht schreibbar.
[[nodiscard]] int runBenchmarkCommand(const Options& options, const nodes::BuiltinLibrary& library,
                                      const std::atomic<bool>& cancel);

}  // namespace whitehole::cli
