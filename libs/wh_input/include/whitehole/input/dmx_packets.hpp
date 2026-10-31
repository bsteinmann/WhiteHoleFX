// dmx_packets.hpp: liest Art-Net- (ArtDmx) und sACN-Pakete (E1.31) und liefert Universe und Kanalwerte.
//
// Warum es das gibt: Das Gegenstück zu output/artnet.hpp und output/sacn.hpp. Wie dort ist der
// Paketaufbau reine Byte-Arbeit ohne Netzwerk, also testbar; die Tests lassen die Ausgabe-Encoder ein
// Paket bauen und lesen es hier wieder. Fremde und defekte Pakete sind im Netz normal (ArtPoll, Sync,
// Preview-Daten, andere Startcodes) und werden still verworfen, nicht als Fehler gemeldet.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "whitehole/input/dmx_store.hpp"

namespace whitehole::input {

struct DmxFrame {
    DmxProtocol protocol{DmxProtocol::ArtNet};
    std::uint32_t universe{0};  // Art-Net: Port-Address 0..32767, sACN: 1..63999
    std::vector<std::uint8_t> channels;
    std::uint8_t priority{100};  // nur sACN
};

/// ArtDmx (OpCode 0x5000, Version >= 14). Leer bei allem anderen.
[[nodiscard]] std::optional<DmxFrame> parseArtNet(std::span<const std::uint8_t> packet);

/// sACN-Datenpaket mit Startcode 0. Leer bei allem anderen, auch bei Preview-Daten (Bit 7 der Optionen)
/// und beim Ende des Datenstroms (Bit 6).
[[nodiscard]] std::optional<DmxFrame> parseSacn(std::span<const std::uint8_t> packet);

}  // namespace whitehole::input
