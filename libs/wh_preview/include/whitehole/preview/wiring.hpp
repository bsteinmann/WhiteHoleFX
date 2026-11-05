// wiring.hpp: Verkabelung und LED-Auswahl in der 3D-Ansicht (Stage Config).
//
// Warum es das gibt: Beim Einrichten will man den Weg des Datenkabels sehen (wo beginnt es, in welche
// Richtung läuft es, wo springt es in die nächste Zeile) und eine einzelne LED anklicken, um ihre Adresse zu
// erfahren. Beides braucht dieselbe Projektion wie das Bild; sie steht hier ohne Qt, damit sie testbar ist und die
// Oberfläche nur noch zeichnet. Die Parameter (Kamera, Bildgröße, Rand) müssen die des gerenderten Bildes sein.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "whitehole/mapping/led_map.hpp"
#include "whitehole/preview/view3d.hpp"

namespace whitehole::preview {

/// Eine LED im Bild.
struct WireDot {
    float x{0.0F};
    float y{0.0F};
    std::uint32_t led{0};  // Index in der LedMap
};

/// Der Kabelweg eines Fixtures im Bild, in Kabelreihenfolge. LEDs hinter der Kamera fehlen.
struct WirePath {
    std::size_t range{0};  // Index in map.ranges
    std::vector<WireDot> dots;
};

/// Kabelwege der gewählten Fixtures (leer = alle).
[[nodiscard]] std::vector<WirePath> projectWiring(const mapping::LedMap& map, const Camera3d& camera,
                                                  std::uint32_t width, std::uint32_t height, std::uint32_t margin,
                                                  std::span<const mapping::FixtureId> fixtures);

struct PickedLed {
    std::uint32_t led{0};            // Index in der LedMap
    std::size_t range{0};            // Index in map.ranges
    std::uint32_t indexInFixture{0}; // 0-basiert, in Kabelreihenfolge
};

/// Die LED, die einem Bildpunkt am nächsten liegt (höchstens `maxDistance` Pixel entfernt). Bei gleichem
/// Abstand gewinnt die näher an der Kamera. Von den gewählten Fixtures (leer = alle).
[[nodiscard]] std::optional<PickedLed> pickLed(const mapping::LedMap& map, const Camera3d& camera, std::uint32_t width,
                                               std::uint32_t height, std::uint32_t margin, float x, float y,
                                               float maxDistance, std::span<const mapping::FixtureId> fixtures);

}  // namespace whitehole::preview
