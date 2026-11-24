// cli_options.hpp: Kommandozeile des Runners.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::cli {

struct Options {
    std::string projectPath;
    bool check{false};
    bool software{false};            // Software-Adapter bevorzugen
    bool help{false};
    bool dmxArtnet{false};           // --dmx-in: Art-Net empfangen
    bool dmxSacn{false};             // --dmx-in: sACN empfangen
    std::string audioIn;             // --audio-in: "default", Geräte-Nummer oder -Name, oder eine .wav-Datei; leer = aus
    bool audioDevices{false};        // --audio-devices: Aufnahmegeräte auflisten und beenden
    bool listNodes{false};           // --list-nodes: alle Node-Typen auflisten und beenden
    std::string describeNode;        // --describe-node <typ>: Ports und Parameter eines Typs ausgeben und beenden
    std::optional<std::uint64_t> frames;  // sonst bis Strg+C
    std::optional<double> fps;            // überschreibt das Projekt
    std::string snapshotPath;             // PNG der LEDs (bei --check: des Layouts)
    std::uint32_t snapshotWidth{960};
    std::uint32_t snapshotHeight{540};
    std::string benchmark;           // --benchmark projection|leds|both; leer = kein Benchmark (braucht kein Projekt)
    bool benchmarkQuick{false};      // --quick: Schnelltest statt vollem Lauf
    std::string benchmarkOut;        // --out <datei.csv|datei.md>: Ergebnis zusätzlich als Datei
};

/// Fehler: InvalidArgument mit einer Meldung, die auf die Hilfe verweist.
[[nodiscard]] Result<Options> parseOptions(const std::vector<std::string>& args);

[[nodiscard]] const char* usageText();

}  // namespace whitehole::cli
