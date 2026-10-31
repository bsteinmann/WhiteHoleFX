// dmx_constants.hpp: die Zahlen, die DMX-Ausgabe (wh_output) und DMX-Eingang (wh_input) teilen.
//
// Warum es diese Datei gibt: Beide Module kennen einander nicht und müssen doch dieselben Ports und
// dieselbe Kanalzahl meinen. Steht eine Zahl an zwei Stellen, ändert sie früher oder später nur eine.
// Abhängigkeiten: keine.
#pragma once

#include <cstddef>
#include <cstdint>

namespace whitehole::dmx {

inline constexpr std::uint16_t kArtNetPort = 6454;        // UDP, Art-Net
inline constexpr std::uint16_t kSacnPort = 5568;          // UDP, sACN (E1.31)
inline constexpr std::size_t kChannelsPerUniverse = 512;  // Kanäle je DMX-Universe

}  // namespace whitehole::dmx
