// project_cue.hpp (privat): Cue-Zeilen der Projektdatei lesen und schreiben.
//
// Warum eine eigene Datei: project_parser.cpp und project_writer.cpp sind schon am Limit der Dateigröße, und die
// Cue-Zeile hat eine eigene kleine Grammatik. Beide Richtungen stehen hier nebeneinander, damit sie zusammenpassen.
//
//   cue [name "Name"] [look "Look"] [trigger go|time <s>|key <Taste>|dmx artnet|sacn <universe> <kanal>|audio]
//       [threshold <1..255>] [delay <s>] [fade <s>]
// Die Einstellungen dürfen in beliebiger Reihenfolge stehen. Ohne look blendet der Cue nach Schwarz.
#pragma once

#include <ostream>

#include "project_text.hpp"
#include "whitehole/project/cue.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

/// Liest eine Cue-Zeile ab dem Wort nach "cue". Fehler stehen im Reader.
[[nodiscard]] Cue parseCue(Reader& r);

void writeCue(std::ostream& out, const Cue& cue);

/// Jeder Cue mit Look muss einen Look der Datei nennen. Fehler: NotFound mit der Cue-Nummer.
[[nodiscard]] Result<void> checkCues(const std::vector<Cue>& cues, const std::vector<Look>& looks);

}  // namespace whitehole::project::detail
