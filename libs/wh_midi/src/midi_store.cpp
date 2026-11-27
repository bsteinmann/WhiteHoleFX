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

void MidiStore::applyRealtime(MidiRealtime event) {
    constexpr std::size_t kTempoTicks = 24;  // eine Viertel: das Tempo mittelt über einen Beat
    constexpr double kGapSeconds = 1.0;      // längere Pause zwischen zwei Clocks: neue Messung
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    switch (event) {
        case MidiRealtime::Start:
            ++clock_.starts;
            clock_.ticks = 0;
            clock_.running = true;
            clockTimes_.clear();
            return;
        case MidiRealtime::Continue:
            clock_.running = true;
            return;
        case MidiRealtime::Stop:
            clock_.running = false;
            return;
        case MidiRealtime::Clock:
            break;
    }
    if (lastClock_ && secondsSince(now, *lastClock_) > kGapSeconds) {
        clockTimes_.clear();
    }
    lastClock_ = now;
    if (clock_.starts == 0) {
        clock_.running = true;  // ohne Start (Gerät lief schon): Clocks zählen trotzdem
    }
    ++clock_.ticks;
    clockTimes_.push_back(now);
    if (clockTimes_.size() > kTempoTicks + 1) {
        clockTimes_.pop_front();
    }
    if (clockTimes_.size() >= 7) {  // ein Viertel Beat genügt für eine erste Schätzung
        const double span = secondsSince(clockTimes_.back(), clockTimes_.front());
        const double perTick = span / static_cast<double>(clockTimes_.size() - 1);
        clock_.bpm = perTick > 0.0 ? 60.0 / (perTick * static_cast<double>(kTempoTicks)) : 0.0;
    }
}

MidiClockState MidiStore::clock() const {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    MidiClockState state = clock_;
    state.ageSeconds = lastClock_ ? secondsSince(now, *lastClock_) : 1e9;
    return state;
}

}  // namespace whitehole::midi
