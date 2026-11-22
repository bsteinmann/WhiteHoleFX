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
    /// Abstand benachbarter LEDs je Eintrag von `ranges` (Welteinheiten, 0 = unbekannt), für die Vorschau.
    std::vector<float> spacing;

    /// Umschließende Box aller Fixtures (Zellen, nicht nur LED-Mitten).
    Box volume;

    /// Bezugsgröße für die Shader-Werte @aspect/@aspectZ/@columns/@rows. Ohne projizierte Fixtures genau `volume` und der
    /// feinste LED-Abstand aller Fixtures (wie immer). Mit Projektion (Fixture::projection): die Fixtures, deren Bild die
    /// Effekte sehen. Es gelten nacheinander Volume-Fixtures (Box aller Volume-Fixtures), sonst Box-Fixtures (Box aller
    /// Box-Fixtures), sonst Faces-Fixtures (die ungedrehte Box des ersten). Pro Bild gibt es nur einen Wert; bei gemischten
    /// Anlagen bleibt der Parameter `aspect`/`cells` der Nodes zum Übersteuern.
    Box frameBox;
    /// Feinster LED-Abstand (Welteinheiten) unter den Fixtures von `frameBox`; 0 = unbekannt.
    float frameSpacing{0.0F};

    [[nodiscard]] std::size_t size() const noexcept { return world.size(); }
};

}  // namespace whitehole::mapping
