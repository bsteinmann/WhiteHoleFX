// region_edit.hpp: das Rechteck des Effektbereichs mit der Maus ziehen (reine Rechnung).
//
// Warum es diese Datei gibt: Die Vorschau lässt den Bereich verschieben, an den Ecken aufziehen
// und neu aufspannen. Die Oberfläche liefert nur, welche Stelle angefasst wurde und wie weit die
// Maus gewandert ist (in Anteilen der Wand, y nach unten wie bei Region); diese Rechnung hält das
// Ergebnis in der Wand und das Rechteck größer als eine Mindestgröße. Ohne Qt, daher testbar.
#pragma once

#include <cstdint>

#include "whitehole/mapping/layout.hpp"

namespace whitehole::mapping {

enum class RegionHandle : std::uint8_t { Move, TopLeft, TopRight, BottomLeft, BottomRight };

/// Kleinste Kantenlänge, die ein gezogener Bereich behält (Anteil der Wand).
inline constexpr float kMinRegionSize = 0.02F;

/// Verschiebt das ganze Rechteck (`Move`, bleibt in der Wand) oder zieht eine Ecke (die
/// gegenüberliegende bleibt stehen). `dx`, `dy` sind Anteile der Wand, y wächst nach unten.
[[nodiscard]] Region dragRegion(const Region& start, RegionHandle handle, float dx, float dy) noexcept;

/// Rechteck aus zwei beliebigen Punkten (Anteile der Wand), auf die Wand begrenzt und mindestens
/// `kMinRegionSize` groß. Für das Aufspannen eines neuen Bereichs.
[[nodiscard]] Region regionFromCorners(float x0, float y0, float x1, float y1) noexcept;

}  // namespace whitehole::mapping
