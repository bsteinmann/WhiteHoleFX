// artnet.hpp: baut ein Art-Net-Paket (ArtDmx) aus Universe und Kanalwerten.
//
// Warum es diese Datei gibt: Art-Net ist das verbreitetste Protokoll, um DMX-Daten per
// UDP an Controller zu schicken. Der Aufbau eines Pakets ist reine Byte-Arbeit und soll
// getrennt vom Socket testbar sein: Die Tests vergleichen die Bytes mit dem Paketformat.
//
// Aufgabe: ArtDmx (OpCode 0x5000). Aufbau, alle Zahlen wie in der Art-Net-Spezifikation:
//   0..7   "Art-Net\0"
//   8..9   OpCode 0x5000, little-endian (Bytes 0x00, 0x50)
//   10..11 Protokollversion 14, big-endian (0x00, 0x0E)
//   12     Sequence (1..255, 0 = Sequenzierung aus)
//   13     Physical (nur zur Information)
//   14..15 Port-Address, little-endian: Byte 14 = SubUni (untere 8 Bit), Byte 15 = Net (obere 7 Bit)
//   16..17 Länge der Daten, big-endian, gerade, 2..512
//   18..   Kanalwerte
// UDP-Port 6454.
//
// Eine ungerade Anzahl Kanäle wird mit einer 0 aufgefüllt, weil die Länge gerade sein
// muss. Abhängigkeiten: nur wh_core (Result).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::output {

inline constexpr std::uint16_t kArtNetPort = 6454;
inline constexpr std::size_t kArtNetHeaderSize = 18;
inline constexpr std::size_t kDmxMaxChannels = 512;
inline constexpr std::uint16_t kArtNetMaxPortAddress = 0x7FFF;  // 15 Bit: Net(7) Sub(4) Uni(4)

struct ArtDmxParams {
    std::uint16_t portAddress{0};  // 0..32767
    std::uint8_t sequence{0};      // 0 = aus, sonst 1..255
    std::uint8_t physical{0};
};

/// Fehler: InvalidArgument bei leeren Daten, mehr als 512 Kanälen oder einer
/// Port-Address über 0x7FFF.
[[nodiscard]] Result<std::vector<std::uint8_t>> encodeArtDmx(const ArtDmxParams& params,
                                                             std::span<const std::uint8_t> channels);

}  // namespace whitehole::output
