// project_destinations.hpp (privat): Protokolle der Controller beim Laden bestimmen.
#pragma once

#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

/// Älteren Dateien fehlt `protocol` in der destination-Zeile: Das Protokoll kam damals vom Patch. Hier bekommt jeder
/// Controller ohne eigene Angabe das Protokoll seiner Patches; benutzen Patches mit verschiedenen Protokollen dieselbe
/// Adresse, entsteht für jedes weitere Protokoll ein Controller mit derselben Adresse (hinten angehängt, die Patches
/// ziehen um). Steht `protocol` in der Datei, muss es zu allen Patches darauf passen (sonst Fehler mit Fixture-Name).
/// `explicitProtocol` hat einen Eintrag je Ziel.
[[nodiscard]] Result<void> resolveDestinationProtocols(Project& project, const std::vector<bool>& explicitProtocol);

}  // namespace whitehole::project::detail
