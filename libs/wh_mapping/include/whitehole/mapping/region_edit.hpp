// region_edit.hpp: das Rechteck eines Region-Nodes mit der Maus ziehen (reine Rechnung).
//
// Warum es diese Datei gibt: Der Node "Region" (wh.transform.region) legt einen Effekt auf ein
// Rechteck der Leinwand. Die Vorschau lässt dieses Rechteck verschieben, an den Ecken ändern und
// neu aufspannen. Die Oberfläche liefert nur, welche Stelle angefasst wurde und wie weit die Maus
// gewandert ist (in Anteilen der Leinwand, y nach unten wie "top" im Node); diese Rechnung hält das
// Ergebnis in der Leinwand und das Rechteck größer als eine Mindestgröße. Ohne Qt, daher testbar.
#pragma once

#include <cstdint>

namespace whitehole::mapping {

/// Rechteck der Leinwand in Anteilen (0..1) von Breite und Höhe; `x` und `y` zählen von links oben
/// (die Parameter "left" und "top" des Region-Nodes, `w` und `h` seine Breite und Höhe).
struct Region {
    float x{0.0F};
    float y{0.0F};
    float w{1.0F};
    float h{1.0F};

    bool operator==(const Region&) const = default;
};

enum class RegionHandle : std::uint8_t { Move, TopLeft, TopRight, BottomLeft, BottomRight };

/// Kleinste Kantenlänge, die ein gezogener Bereich behält (Anteil der Leinwand).
inline constexpr float kMinRegionSize = 0.02F;

/// Verschiebt das ganze Rechteck (`Move`, bleibt in der Leinwand) oder zieht eine Ecke (die
/// gegenüberliegende bleibt stehen). `dx`, `dy` sind Anteile der Leinwand, y wächst nach unten.
[[nodiscard]] Region dragRegion(const Region& start, RegionHandle handle, float dx, float dy) noexcept;

/// Rechteck aus zwei beliebigen Punkten (Anteile der Leinwand), auf die Leinwand begrenzt und mindestens
/// `kMinRegionSize` groß. Für das Aufspannen eines neuen Bereichs.
[[nodiscard]] Region regionFromCorners(float x0, float y0, float x1, float y1) noexcept;

}  // namespace whitehole::mapping
