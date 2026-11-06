// connection_scale.hpp: Vorschlag für die Skalierung am Kabel.
//
// Warum es das gibt: Viele Quellen liefern Werte von 0 bis 1 (Pegel, Pulse, Position). Ein Winkel braucht 0 bis 360,
// ein Parameter mit Bereich dessen Grenzen. Die Oberfläche bietet dafür einen Knopf "An Bereich anpassen"; was er
// einträgt, wird hier berechnet, damit es testbar bleibt und nicht im Qt-Code steckt.
#pragma once

#include <optional>
#include <utility>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

/// Faktor und Offset, die einen Wert von 0 bis 1 über den ganzen Bereich des Parameters spannen:
///  - mit Minimum und Maximum: Faktor = Maximum - Minimum, Offset = Minimum
///  - ohne Bereich, aber ein Winkel (Schlüssel enthält "angle" oder "rotation"): Faktor 360, Offset 0
/// Nichts bei Parametern, die keine Zahl sind, und bei solchen ohne erkennbaren Bereich.
[[nodiscard]] std::optional<std::pair<double, double>> suggestConnectionScale(const ParameterInfo& parameter);

}  // namespace whitehole::runtime
