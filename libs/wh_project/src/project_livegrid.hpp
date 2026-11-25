// project_livegrid.hpp (privat): die Zeilen `livegrid` und `cell` der Projektdatei lesen und schreiben.
//
// Warum eine eigene Datei: project_parser.cpp und project_writer.cpp sind schon über dem Limit der Dateigröße, und das
// Raster hat eine eigene kleine Grammatik. Beide Richtungen stehen hier nebeneinander, damit sie zusammenpassen.
//
//   livegrid <spalten> <zeilen>
//   cell <zeile> <spalte> look "<Name>" [flash] [in <s>] [return <s>]
// Zeile und Spalte zählen ab 1. Nur belegte Felder stehen in der Datei; `in`/`return` nur bei `flash`.
#pragma once

#include <ostream>

#include "project_text.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

/// Liest die Zeile `livegrid <spalten> <zeilen>` (nach dem Schlüsselwort) und legt das leere Raster an.
/// Fehler: zweimal `livegrid`, Größe außerhalb 1..16.
[[nodiscard]] LiveGrid parseLiveGridHeader(Reader& r, const LiveGrid& existing);

/// Liest eine `cell`-Zeile (nach dem Schlüsselwort) ins Raster. Fehler: `cell` vor `livegrid`, Feld außerhalb, doppelt.
void parseLiveCell(Reader& r, LiveGrid& grid);

/// Der Block `livegrid ...` samt `cell`-Zeilen (mit Zeilenenden). Leer, wenn kein Raster definiert ist.
[[nodiscard]] std::string formatLiveGrid(const LiveGrid& grid);

/// Das Raster muss zur Größe passen und jedes belegte Feld einen Look der Datei nennen.
/// Fehler: InvalidArgument (Größe) oder NotFound (Look fehlt).
[[nodiscard]] Result<void> checkLiveGrid(const LiveGrid& grid, const std::vector<Look>& looks);

}  // namespace whitehole::project::detail
