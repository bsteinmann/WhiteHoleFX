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
    ++stats_.messages;
    const Stamped stamped{message, now, stats_.messages};
    last_ = stamped;
    if (message.kind == MessageKind::ControlChange) {
        lastCc_ = stamped;
    } else if (message.kind == MessageKind::NoteOn) {
        lastNoteOn_ = stamped;
    }
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

std::optional<LastMessage> MidiStore::snapshot(const std::optional<Stamped>& stamped) const {
    if (!stamped) {
        return std::nullopt;
    }
    return LastMessage{stamped->message, secondsSince(now_(), stamped->time), stamped->sequence};
}

std::optional<LastMessage> MidiStore::last() const {
    const std::lock_guard lock{mutex_};
    return snapshot(last_);
}

std::optional<LastMessage> MidiStore::lastControlChange() const {
    const std::lock_guard lock{mutex_};
    return snapshot(lastCc_);
}

std::optional<LastMessage> MidiStore::lastNoteOn() const {
    const std::lock_guard lock{mutex_};
    return snapshot(lastNoteOn_);
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
