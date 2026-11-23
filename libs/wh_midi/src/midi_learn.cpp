// midi_learn.cpp: Umsetzung zu midi_learn.hpp.
#include "whitehole/midi/midi_learn.hpp"

namespace whitehole::midi {

std::optional<LastMessage> MidiLearn::latest() const {
    return want_ == Want::Controller ? store_->lastControlChange() : store_->lastNoteOn();
}

void MidiLearn::arm(Want want) {
    want_ = want;
    const auto current = latest();
    baseline_ = current ? current->sequence : 0;
    armed_ = true;
}

std::optional<MidiMessage> MidiLearn::poll() {
    if (!armed_) {
        return std::nullopt;
    }
    const auto current = latest();
    if (!current || current->sequence <= baseline_) {
        return std::nullopt;
    }
    armed_ = false;
    return current->message;
}

}  // namespace whitehole::midi
