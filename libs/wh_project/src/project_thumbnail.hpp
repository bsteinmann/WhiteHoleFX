// project_thumbnail.hpp (privat): die Zeile `thumb <breite> <höhe> <hex>` eines look-Blocks lesen und schreiben.
//
// Warum eine eigene Datei: project_parser.cpp und project_writer.cpp sind schon über dem Limit der Dateigröße, und das
// Standbild hat eine eigene kleine Kodierung (Hex-Text, damit die Projektdatei reiner Text bleibt).
#pragma once

#include <optional>
#include <string>

#include "project_text.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

/// Liest `thumb <breite> <höhe> <hex>` (nach dem Schlüsselwort). Setzt im Fehlerfall `r.fail(...)`.
[[nodiscard]] LookThumbnail parseThumbnail(Reader& r);

/// Die komplette Zeile `  thumb ...\n` (mit Zeilenende).
[[nodiscard]] std::string formatThumbnail(const LookThumbnail& thumbnail);

}  // namespace whitehole::project::detail
