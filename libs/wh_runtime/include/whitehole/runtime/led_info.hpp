// led_info.hpp: "Was ist das für eine LED?" für die Vorschau.
//
// Warum es das gibt: Beim Einrichten fragt man sich oft, welche LED da gerade falsch leuchtet:
// zu welchem Fixture sie gehört, die wievielte am Kabel sie ist und wohin ihre Daten gehen (Universe
// und Kanal). Beim Überfahren der Vorschau mit der Maus zeigt das Studio diese Angaben. Die Rechnung
// (Treffer im Bild, Adresse aus dem Ausgabeplan, Text) steht hier ohne Qt und ist testbar.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "whitehole/core/types.hpp"
#include "whitehole/mapping/led_map.hpp"
#include "whitehole/output/output_plan.hpp"
#include "whitehole/preview/view.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

/// Wohin die Daten einer LED gehen.
struct LedAddress {
    output::Protocol protocol{output::Protocol::ArtNet};
    std::uint32_t destination{0};
    std::uint32_t universe{0};      // Art-Net/sACN
    std::uint32_t firstChannel{0};  // Art-Net/sACN: 1-basiert, die LED belegt firstChannel..firstChannel+2
    std::uint32_t byteOffset{0};    // DDP: Byte-Offset im Strom
};

struct LedInfo {
    std::uint32_t index{0};             // in der LedMap (alle Fixtures hintereinander)
    mapping::FixtureId fixture;
    std::string fixtureName;
    std::uint32_t indexInFixture{0};    // am Kabel, 0 = erste
    std::uint32_t fixtureLedCount{0};
    Vec3 world;
    std::optional<ColorRgba> color;     // Farbe des letzten Frames, leer ohne Lauf
    std::optional<LedAddress> address;  // leer, wenn das Fixture nicht gepatcht ist
};

/// Die LED, deren Bildpunkt dem Punkt (pixelX, pixelY) am nächsten liegt, höchstens `maxDistance`
/// Pixel entfernt. Bei Gleichstand gewinnt die spätere (die weiter vorn gezeichnete).
[[nodiscard]] std::optional<std::uint32_t> ledAtPixel(const mapping::LedMap& map, const preview::ViewFrame& frame,
                                                      float pixelX, float pixelY, float maxDistance = 10.0F);

/// Beschreibt LED `index`. `colors` darf leer sein. Leer, wenn der Index nicht zur Karte passt.
/// Die Adresse kommt aus dem Ausgabeplan des Projekts (Fixture ohne Patch: keine Adresse).
[[nodiscard]] std::optional<LedInfo> describeLed(const project::Project& project, const mapping::LedMap& map,
                                                 std::uint32_t index, std::span<const ColorRgba> colors);

/// Mehrzeiliger Text für einen Tooltip.
[[nodiscard]] std::string formatLedInfo(const LedInfo& info);

}  // namespace whitehole::runtime
