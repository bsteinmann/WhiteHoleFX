// layout.hpp: alle Fixtures eines Projekts und ihre Abbildung auf den Einheitswürfel.
//
// Warum es diese Datei gibt: Effekte arbeiten im Einheitswürfel, die Hardware im
// Raum. Das Layout verwaltet die Fixtures (hinzufügen, verschieben, ersetzen,
// entfernen) und "backt" sie zu einer LedMap, in der jede LED ihre Würfelkoordinate hat.
//
// Normalisierung (NormalizeMode):
//  - Stretch (Standard): Die Box aller Fixtures wird je Achse auf [0,1] gestreckt. Ein
//    16x8-Panel füllt den ganzen Canvas, genau wie die Textur des Canvas-Passes.
//  - KeepAspect: Alle Achsen werden mit demselben Faktor skaliert und der Inhalt wird
//    im Würfel zentriert. Ein Kreis bleibt ein Kreis, auch bei einer breiten Wand.
//  In beiden Fällen bleibt eine Achse ohne Ausdehnung bei 0 (flache Anlage: p.z = 0).
//
// Threading: ein Layout gehört einem Thread. Die LedMap ist ein Wert und wandert als
// Kopie zum Engine-Thread.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/mapping/led_map.hpp"

namespace whitehole::mapping {

enum class NormalizeMode : std::uint8_t { Stretch, KeepAspect };

/// Obergrenze für alle LEDs eines Layouts (passt in 32 Bit, etwa 8 Mio.).
inline constexpr std::uint64_t kMaxLedsPerLayout = 1U << 23;

class Layout {
public:
    /// Fehler: InvalidArgument (Form ungültig, Ursprung nicht endlich).
    Result<FixtureId> add(std::string name, const Vec3& origin, FixtureShape shape);

    /// Fehler: NotFound.
    Result<void> remove(FixtureId id);
    Result<void> move(FixtureId id, const Vec3& origin);
    /// Tauscht die Form; bei einem Fehler bleibt die alte erhalten.
    Result<void> replaceShape(FixtureId id, FixtureShape shape);

    [[nodiscard]] const Fixture* find(FixtureId id) const noexcept;
    [[nodiscard]] const std::vector<Fixture>& fixtures() const noexcept { return fixtures_; }

    /// Fehler: InvalidArgument bei leerem Layout oder zu vielen LEDs.
    [[nodiscard]] Result<LedMap> bake(NormalizeMode mode = NormalizeMode::Stretch) const;

private:
    std::vector<Fixture> fixtures_;
    IdGenerator<FixtureId> ids_;
};

}  // namespace whitehole::mapping
