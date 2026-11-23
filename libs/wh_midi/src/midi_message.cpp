// midi_message.cpp: Auswertung und Aufbau von MIDI-Kanalnachrichten (siehe midi_message.hpp).
#include "whitehole/midi/midi_message.hpp"

namespace whitehole::midi {

std::optional<MidiMessage> parseMidi(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < 3) {
        return std::nullopt;
    }
    const std::uint8_t status = bytes[0];
    const std::uint8_t data1 = bytes[1];
    const std::uint8_t data2 = bytes[2];
    if (data1 > 127 || data2 > 127) {
        return std::nullopt;
    }
    MidiMessage message;
    message.channel = static_cast<std::uint8_t>(status & 0x0F);
    message.data1 = data1;
    message.data2 = data2;
    switch (status & 0xF0) {
        case 0x80:
            message.kind = MessageKind::NoteOff;
            break;
        case 0x90:
            message.kind = data2 == 0 ? MessageKind::NoteOff : MessageKind::NoteOn;
            break;
        case 0xB0:
            message.kind = MessageKind::ControlChange;
            break;
        default:
            return std::nullopt;
    }
    return message;
}

MidiBytes toBytes(const MidiMessage& message) noexcept {
    std::uint8_t high = 0x90;
    switch (message.kind) {
        case MessageKind::NoteOff:
            high = 0x80;
            break;
        case MessageKind::NoteOn:
            high = 0x90;
            break;
        case MessageKind::ControlChange:
            high = 0xB0;
            break;
    }
    return {static_cast<std::uint8_t>(high | (message.channel & 0x0F)), message.data1, message.data2};
}

MidiMessage noteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) noexcept {
    return {MessageKind::NoteOn, channel, note, velocity};
}

MidiMessage noteOff(std::uint8_t channel, std::uint8_t note) noexcept {
    return {MessageKind::NoteOff, channel, note, 0};
}

MidiMessage controlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value) noexcept {
    return {MessageKind::ControlChange, channel, controller, value};
}

}  // namespace whitehole::midi
