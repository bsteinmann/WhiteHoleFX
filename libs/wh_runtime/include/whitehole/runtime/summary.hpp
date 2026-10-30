// summary.hpp: Prüfbericht eines Projekts, ohne GPU und ohne Netzwerk.
//
// Warum es diese Datei gibt: Beim Patchen passieren die meisten Fehler (Universe doppelt belegt,
// Ziel vergessen, Strip zu lang). Der Bericht baut denselben Ausgabeplan wie der Betrieb und
// beschreibt ihn in Worten, bevor ein Paket verschickt wird.
#pragma once

#include <string>

#include "whitehole/core/result.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

/// Fehler: dieselben wie buildOutput.
[[nodiscard]] Result<std::string> summarize(const project::Project& project);

}  // namespace whitehole::runtime
