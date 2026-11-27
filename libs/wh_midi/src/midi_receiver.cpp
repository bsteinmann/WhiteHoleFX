// midi_receiver.cpp: Empfänger zwischen MIDI-Eingang und MidiStore (siehe midi_receiver.hpp).
#include "whitehole/midi/midi_receiver.hpp"

namespace whitehole::midi {

MidiReceiver::MidiReceiver(std::unique_ptr<IMidiInput> input, std::shared_ptr<MidiStore> store)
    : input_{std::move(input)}, store_{std::move(store)} {}

MidiReceiver::~MidiReceiver() { stop(); }

Result<void> MidiReceiver::start(const std::string& portName) {
    stop();
    if (portName.empty()) {
        return {};
    }
    const std::shared_ptr<MidiStore> store = store_;
    input_->setRealtimeHandler([store](MidiRealtime event) { store->applyRealtime(event); });
    return input_->open(portName, [store](const MidiMessage& message) { store->apply(message); });
}

void MidiReceiver::stop() { input_->close(); }

}  // namespace whitehole::midi
