// point_file.hpp: eine Datei mit LED-Positionen (CSV oder JSON) lesen.
//
// Warum es das gibt: Die Formatlogik steht in wh_mapping (point_import.hpp) und kennt keine Dateien. Die
// Oberfläche und die Tests brauchen den Weg von einem Dateipfad zu den Punkten, samt verständlicher
// Fehlermeldung mit dem Dateinamen. Dieser Weg ist hier, ohne Qt.
#pragma once

#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/mapping/point_import.hpp"

namespace whitehole::runtime {

/// Das Format folgt der Endung (.csv, .txt, .tsv, .json). Fehler: IoError (Datei nicht lesbar),
/// InvalidArgument (Format) - die Meldung beginnt mit dem Dateinamen.
[[nodiscard]] Result<std::vector<Vec3>> loadPointsFile(const std::string& path, const mapping::ImportOptions& options = {});

/// Der Dateiname ohne Ordner und Endung, als Vorschlag für den Namen des Fixtures.
[[nodiscard]] std::string pointsFixtureName(const std::string& path);

}  // namespace whitehole::runtime
