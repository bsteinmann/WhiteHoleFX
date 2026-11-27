// midi_message.hpp: eine MIDI-Kanalnachricht (Note an/aus, Regler) als kleiner Werttyp.
//
// Warum es das gibt: Store, Backend und Nodes sollen dieselbe Sprache sprechen, ohne rohe Bytes herumzureichen.
// Hier steckt nur die Auswertung von Statusbyte und Datenbytes, keine Geräteanbindung - alles ist ohne Hardware testbar.
// Betrachtet werden nur Kanalnachrichten (Note an/aus, Control Change); Systemnachrichten (Clock, SysEx, ...) liefert
// parse() als "nicht unterstützt" (nullopt), damit sie den Zustand nie verfälschen.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace whitehole::midi {

enum class MessageKind : std::uint8_t { NoteOff, NoteOn, ControlChange };

struct MidiMessage {
    MessageKind kind{MessageKind::NoteOn};
    std::uint8_t channel{0};  // 0..15 (an Geräten meist als 1..16 beschriftet)
    std::uint8_t data1{0};    // Notennummer bzw. CC-Nummer, 0..127
    std::uint8_t data2{0};    // Velocity bzw. CC-Wert, 0..127

    bool operator==(const MidiMessage&) const = default;
};

/// Liest rohe Bytes. Ein Note-On mit Velocity 0 gilt als Note-Off (so senden viele Geräte). Leer bei unbekanntem Status,
/// zu kurzen Daten oder Datenbytes über 127.
[[nodiscard]] std::optional<MidiMessage> parseMidi(std::span<const std::uint8_t> bytes) noexcept;

/// Die drei Bytes der Nachricht (Status, data1, data2), wie sie ein Gerät erwartet.
struct MidiBytes {
    std::uint8_t status{0};
    std::uint8_t data1{0};
    std::uint8_t data2{0};
};
[[nodiscard]] MidiBytes toBytes(const MidiMessage& message) noexcept;

/// MIDI-Echtzeitmeldungen für die Tempo-Uhr (MIDI-Clock): Clock (0xF8, 24 je Viertel), Start (0xFA), Continue (0xFB),
/// Stop (0xFC). Bewusst getrennt von MidiMessage: sie haben keinen Kanal und keine Daten, und Noten/Regler-Code bleibt unberührt.
enum class MidiRealtime : std::uint8_t { Clock, Start, Continue, Stop };
/// Liest ein einzelnes Echtzeit-Byte. Leer bei allem anderen.
[[nodiscard]] std::optional<MidiRealtime> parseMidiRealtime(std::span<const std::uint8_t> bytes) noexcept;

[[nodiscard]] MidiMessage noteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) noexcept;
[[nodiscard]] MidiMessage noteOff(std::uint8_t channel, std::uint8_t note) noexcept;
[[nodiscard]] MidiMessage controlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value) noexcept;

}  // namespace whitehole::midi
