// midi_store.cpp: Zustand und Ereignisliste der MIDI-Eingabe (siehe midi_store.hpp).
#include "whitehole/midi/midi_store.hpp"

namespace whitehole::midi {

namespace {
double secondsSince(std::chrono::steady_clock::time_point now, std::chrono::steady_clock::time_point then) {
    return std::chrono::duration<double>(now - then).count();
}
}  // namespace

void MidiStore::apply(const MidiMessage& message) {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    const std::size_t i = index(message.channel, message.data1);
    if (message.kind == MessageKind::ControlChange) {
        controllers_[i] = Slot{true, false, message.data2, now};
    } else {
        Slot& note = notes_[i];
        note.seen = true;
        note.on = message.kind == MessageKind::NoteOn;
        if (note.on) {
            note.value = message.data2;
        }
        note.updated = now;
    }
    events_.push_back(message);
    while (events_.size() > kMaxQueuedEvents) {
        events_.pop_front();
        ++stats_.dropped;
    }
    last_ = message;
    lastTime_ = now;
    ++stats_.messages;
}

std::optional<CcSample> MidiStore::readCc(std::uint8_t channel, std::uint8_t controller) const {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    const Slot& slot = controllers_[index(channel, controller)];
    if (!slot.seen) {
        return std::nullopt;
    }
    return CcSample{slot.value, secondsSince(now, slot.updated)};
}

std::optional<NoteSample> MidiStore::readNote(std::uint8_t channel, std::uint8_t note) const {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    const Slot& slot = notes_[index(channel, note)];
    if (!slot.seen) {
        return std::nullopt;
    }
    return NoteSample{slot.on, slot.value, secondsSince(now, slot.updated)};
}

std::optional<LastMessage> MidiStore::last() const {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    if (!last_) {
        return std::nullopt;
    }
    return LastMessage{*last_, secondsSince(now, lastTime_)};
}

std::vector<MidiMessage> MidiStore::takeEvents() {
    const std::lock_guard lock{mutex_};
    std::vector<MidiMessage> out{events_.begin(), events_.end()};
    events_.clear();
    return out;
}

MidiStats MidiStore::stats() const {
    const std::lock_guard lock{mutex_};
    return stats_;
}

}  // namespace whitehole::midi
