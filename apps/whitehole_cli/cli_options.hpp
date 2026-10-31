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
    std::optional<std::uint64_t> frames;  // sonst bis Strg+C
    std::optional<double> fps;            // überschreibt das Projekt
    std::string snapshotPath;             // PNG der LEDs (bei --check: des Layouts)
    std::uint32_t snapshotWidth{960};
    std::uint32_t snapshotHeight{540};
};

/// Fehler: InvalidArgument mit einer Meldung, die auf die Hilfe verweist.
[[nodiscard]] Result<Options> parseOptions(const std::vector<std::string>& args);

[[nodiscard]] const char* usageText();

}  // namespace whitehole::cli
