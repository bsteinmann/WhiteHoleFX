// sacn.hpp: baut ein sACN-Paket (ANSI E1.31, "Streaming ACN") für ein DMX-Universe.
//
// Warum es diese Datei gibt: sACN ist das Standardprotokoll großer Lichtanlagen und
// ersetzt Art-Net dort, wo Priorität, Multicast und eine feste Spezifikation wichtig
// sind. Wie bei Art-Net ist der Paketaufbau reine Byte-Arbeit und ohne Netzwerk testbar.
//
// Aufgabe: ein Datenpaket mit drei ineinander liegenden Schichten. Offsets (Bytes):
//   Root-Layer
//     0..1   Preamble-Größe 0x0010
//     2..3   Postamble-Größe 0
//     4..15  "ASC-E1.17\0\0\0"
//     16..17 Flags und Länge: 0x7000 | (Gesamtgröße - 16)
//     18..21 Vektor 0x00000004 (Datenpaket)
//     22..37 CID (16 Byte, die ID dieses Senders)
//   Framing-Layer
//     38..39 Flags und Länge: 0x7000 | (Gesamtgröße - 38)
//     40..43 Vektor 0x00000002
//     44..107 Quellname (64 Byte, UTF-8, mit 0 aufgefüllt)
//     108    Priorität (0..200, Standard 100)
//     109..110 Sync-Adresse (0 = keine)
//     111    Sequenznummer
//     112    Optionen
//     113..114 Universe (1..63999)
//   DMP-Layer
//     115..116 Flags und Länge: 0x7000 | (Gesamtgröße - 115)
//     117    Vektor 0x02
//     118    Adresstyp 0xA1
//     119..120 Erste Adresse 0
//     121..122 Adressschritt 1
//     123..124 Anzahl Werte = Kanäle + 1
//     125    DMX-Startcode 0
//     126..  Kanalwerte
// Ein volles Universe (512 Kanäle) ist 638 Byte lang. UDP-Port 5568, Multicast-Gruppe
// 239.255.<Universe hoch>.<Universe niedrig>.
//
// Abhängigkeiten: nur wh_core (Result).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/dmx_constants.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/output/artnet.hpp"

namespace whitehole::output {

inline constexpr std::uint16_t kSacnPort = dmx::kSacnPort;
inline constexpr std::size_t kSacnHeaderSize = 126;  // bis einschließlich Startcode
inline constexpr std::uint16_t kSacnMaxUniverse = 63999;
inline constexpr std::size_t kSacnSourceNameSize = 64;

struct SacnParams {
    std::array<std::uint8_t, 16> cid{};  // eindeutige Sender-ID
    std::string sourceName{"WhiteHoleFX"};  // wird auf 63 Byte gekürzt
    std::uint16_t universe{1};              // 1..63999
    std::uint8_t priority{100};             // 0..200
    std::uint8_t sequence{0};
};

/// Fehler: InvalidArgument bei leeren Daten, mehr als 512 Kanälen, Universe außerhalb
/// von 1..63999 oder Priorität über 200.
[[nodiscard]] Result<std::vector<std::uint8_t>> encodeSacn(const SacnParams& params,
                                                           std::span<const std::uint8_t> channels);

/// Die Multicast-Adresse eines Universe als Text, z. B. "239.255.0.1".
[[nodiscard]] std::string sacnMulticastAddress(std::uint16_t universe);

}  // namespace whitehole::output
