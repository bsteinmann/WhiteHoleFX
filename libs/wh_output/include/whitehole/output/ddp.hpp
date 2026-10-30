// ddp.hpp: baut DDP-Pakete (Distributed Display Protocol) für RGB-Pixeldaten.
//
// Warum es diese Datei gibt: DDP schickt Pixelbytes direkt mit einem Byte-Offset, ohne
// Universen. Es ist das schlankste Protokoll für WLED und viele ESP-Controller und
// vermeidet die 512-Kanal-Grenze: ein Strip mit 1000 LEDs ist ein einziger Strom.
//
// Aufgabe: Header (10 Byte) plus Daten, höchstens 1440 Datenbytes pro Paket:
//   0     Flags: 0x40 (Version 1) | 0x01 (Push, "jetzt anzeigen")
//   1     Sequenznummer 1..15 (0 = keine)
//   2     Datentyp 0x0B (RGB, 8 Bit je Kanal)
//   3     Ziel-ID 1 (Standard-Ausgabegerät)
//   4..7  Byte-Offset der Daten im Pixelstrom, big-endian
//   8..9  Datenlänge, big-endian
// UDP-Port 4048. Größere Datenmengen werden in mehrere Pakete geschnitten, nur das
// letzte trägt das Push-Flag, damit der Controller erst das vollständige Bild zeigt.
//
// Abhängigkeiten: nur wh_core (Result).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::output {

inline constexpr std::uint16_t kDdpPort = 4048;
inline constexpr std::size_t kDdpHeaderSize = 10;
inline constexpr std::size_t kDdpMaxPayload = 1440;  // durch 3 teilbar: keine LED wird geteilt

struct DdpParams {
    std::uint32_t offset{0};     // Byte-Offset im Pixelstrom
    std::uint8_t sequence{0};    // 0 oder 1..15
    std::uint8_t destination{1};
    bool push{true};
};

/// Ein Paket. Fehler: InvalidArgument bei leeren Daten, mehr als 1440 Bytes oder
/// Sequenz über 15.
[[nodiscard]] Result<std::vector<std::uint8_t>> encodeDdp(const DdpParams& params,
                                                          std::span<const std::uint8_t> pixels);

/// Schneidet `pixels` in Pakete zu höchstens 1440 Bytes (Offsets laufen weiter, ab
/// `params.offset`); `params.push` gilt nur für das letzte Paket. Fehler wie encodeDdp,
/// außerdem wenn offset + Länge die 32 Bit überschreitet.
[[nodiscard]] Result<std::vector<std::vector<std::uint8_t>>> encodeDdpStream(
    const DdpParams& params, std::span<const std::uint8_t> pixels);

}  // namespace whitehole::output
