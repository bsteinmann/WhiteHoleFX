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
// Effektbereich (Region): Soll der Effekt nur einen Teil der Wand füllen (Bild-in-Bild), gibt man
// ein Rechteck an. Die Normalisierung gilt dann für dieses Rechteck statt für die ganze Wand, und
// LEDs außerhalb stehen in LedMap::outside. Der Abtast-Pass macht sie durchsichtig (Alpha 0),
// die LEDs bleiben also aus. Das Rechteck steht in Anteilen der Wandbox (x, y von der linken
// oberen Ecke, wie Zeilen und Spalten einer Matrix); die z-Achse bleibt unberührt.
//
// Threading: ein Layout gehört einem Thread. Die LedMap ist ein Wert und wandert als
// Kopie zum Engine-Thread.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/mapping/led_map.hpp"

namespace whitehole::mapping {

enum class NormalizeMode : std::uint8_t { Stretch, KeepAspect };

/// Rechteck der Wand, in dem der Effekt läuft. Alle Werte sind Anteile (0..1) von Breite und Höhe
/// der Wandbox, `x` und `y` zählen von links oben.
struct Region {
    float x{0.0F};
    float y{0.0F};
    float w{1.0F};
    float h{1.0F};

    bool operator==(const Region&) const = default;
};

/// Prüft: Zahlen endlich, `w` und `h` größer 0, das Rechteck liegt in der Wand
/// (x, y >= 0, x + w <= 1, y + h <= 1). Fehler: InvalidArgument.
[[nodiscard]] Result<void> validateRegion(const Region& region);

/// Das Rechteck des Bereichs in Weltkoordinaten (z wie die Wand). `wall` ist `LedMap::volume`.
[[nodiscard]] Box regionBox(const Box& wall, const Region& region) noexcept;

/// Obergrenze für alle LEDs eines Layouts (passt in 32 Bit, etwa 8 Mio.).
inline constexpr std::uint64_t kMaxLedsPerLayout = 1U << 23;

class Layout {
public:
    /// Fehler: InvalidArgument (Form ungültig, Ursprung nicht endlich).
    Result<FixtureId> add(std::string name, const Vec3& origin, FixtureShape shape);

    /// Fehler: NotFound.
    Result<void> remove(FixtureId id);
    Result<void> move(FixtureId id, const Vec3& origin);
    /// Der Name wird nicht auf Eindeutigkeit geprüft (das tut die Projektebene). Fehler: NotFound.
    Result<void> rename(FixtureId id, std::string name);
    /// Tauscht die Form; bei einem Fehler bleibt die alte erhalten.
    Result<void> replaceShape(FixtureId id, FixtureShape shape);

    [[nodiscard]] const Fixture* find(FixtureId id) const noexcept;
    [[nodiscard]] const std::vector<Fixture>& fixtures() const noexcept { return fixtures_; }

    /// Fehler: InvalidArgument bei leerem Layout, zu vielen LEDs oder ungültigem Bereich.
    /// Mit `region` gilt die Normalisierung für den Bereich und `LedMap::outside` ist gesetzt.
    [[nodiscard]] Result<LedMap> bake(NormalizeMode mode = NormalizeMode::Stretch,
                                      const std::optional<Region>& region = std::nullopt) const;

private:
    std::vector<Fixture> fixtures_;
    IdGenerator<FixtureId> ids_;
};

}  // namespace whitehole::mapping
