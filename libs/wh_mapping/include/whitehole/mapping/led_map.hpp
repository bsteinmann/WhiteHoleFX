// led_map.hpp: das gebackene Ergebnis eines Layouts.
//
// Warum es diese Datei gibt: Der Abtast-Pass und die Ausgabe brauchen keine Fixtures,
// sondern nur flache Listen: "LED Nummer i sitzt bei p" und "diese Fixtures belegen
// diese Bereiche". Die LedMap ist genau das, unveränderlich und leicht auf die GPU zu
// kopieren.
#pragma once

#include <cstdint>
#include <vector>

#include "whitehole/core/types.hpp"
#include "whitehole/mapping/fixture.hpp"

namespace whitehole::mapping {

/// Zusammenhängender Bereich der LedMap, der zu einem Fixture gehört.
struct FixtureRange {
    FixtureId fixture;
    std::uint32_t first{0};
    std::uint32_t count{0};

    bool operator==(const FixtureRange&) const = default;
};

struct LedMap {
    /// Position jeder LED im Raum, in Fixture-Reihenfolge und innerhalb eines Fixtures
    /// in Kabelreihenfolge.
    std::vector<Vec3> world;

    /// Dieselben LEDs im Einheitswürfel [0,1]^3. Das sind die Koordinaten p, die der
    /// Effekt sieht. Eine Achse ohne Ausdehnung (flache Matrix) ergibt 0.
    std::vector<Vec3> unit;

    std::vector<FixtureRange> ranges;

    /// Umschließende Box aller Fixtures (Zellen, nicht nur LED-Mitten).
    Box volume;

    /// Pro LED 1, wenn sie außerhalb des Effektbereichs liegt (siehe Region in layout.hpp).
    /// Leer, wenn kein Bereich gesetzt ist; sonst so lang wie `world`.
    std::vector<std::uint8_t> outside;

    [[nodiscard]] std::size_t size() const noexcept { return world.size(); }
};

}  // namespace whitehole::mapping
