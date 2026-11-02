// cue_runner.cpp: Zustandsmaschine der Cuelist (siehe cue_runner.hpp).
#include "whitehole/runtime/cue_runner.hpp"

#include <algorithm>
#include <cctype>

namespace whitehole::runtime {

namespace {

bool sameKey(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

}  // namespace

void CueRunner::load(std::vector<project::Cue> cues) {
    cues_ = std::move(cues);
    stop();
}

bool CueRunner::start(double now) {
    stop();
    if (cues_.empty()) {
        return false;
    }
    running_ = true;
    current_ = 0;
    fadeStart_ = now;
    fadeEnd_ = now;  // Cue 0 steht sofort; der Time-Auslöser des zweiten zählt ab jetzt
    return true;
}

void CueRunner::stop() {
    running_ = false;
    current_ = -1;
    pending_ = -1;
    dmxFor_ = -1;
    dmxHigh_ = false;
    fadeStart_ = 0.0;
    fadeEnd_ = 0.0;
}

int CueRunner::nextIndex() const noexcept {
    const int next = current_ + 1;
    return next < static_cast<int>(cues_.size()) ? next : -1;
}

void CueRunner::trigger(int index, double now) {
    const project::Cue& cue = cues_[static_cast<std::size_t>(index)];
    current_ = index;
    pending_ = index;
    pendingDue_ = now + cue.delay;
    fadeStart_ = pendingDue_;
    fadeEnd_ = pendingDue_ + cue.fade;
    dmxFor_ = -1;  // der nächste Cue merkt sich den Zustand seines Kanals neu
}

bool CueRunner::fire(int index, double now) {
    if (!running_ || index < 0 || index >= static_cast<int>(cues_.size())) {
        return false;
    }
    trigger(index, now);
    return true;
}

std::optional<int> CueRunner::update(double now, const CueEvents& events) {
    if (!running_) {
        return std::nullopt;
    }
    // 1. Tasten springen zu "ihrem" Cue, von jedem Stand aus.
    for (const std::string& key : events.keys) {
        for (std::size_t i = 0; i < cues_.size(); ++i) {
            if (cues_[i].trigger == project::CueTrigger::Key && sameKey(cues_[i].key, key)) {
                trigger(static_cast<int>(i), now);
                break;
            }
        }
    }
    // 2. Der nächste Cue wartet auf seinen Auslöser (GO löst immer aus).
    if (const int next = nextIndex(); next >= 0) {
        const project::Cue& cue = cues_[static_cast<std::size_t>(next)];
        bool fires = events.go;
        switch (cue.trigger) {
            case project::CueTrigger::Go: break;
            case project::CueTrigger::Time: fires = fires || now >= fadeEnd_ + cue.time; break;
            case project::CueTrigger::Key: break;
            case project::CueTrigger::Audio: fires = fires || events.beat; break;
            case project::CueTrigger::Dmx: {
                const std::optional<int> value = events.dmx ? events.dmx(cue) : std::nullopt;
                const bool high = value && *value >= static_cast<int>(cue.dmxThreshold);
                if (dmxFor_ != next) {
                    dmxFor_ = next;  // erster Blick auf diesen Kanal: ein schon hoher Regler löst nicht aus
                    dmxHigh_ = high;
                } else {
                    fires = fires || (high && !dmxHigh_);
                    dmxHigh_ = high;
                }
                break;
            }
        }
        if (fires) {
            trigger(next, now);
        }
    }
    // 3. Ist die Wartezeit des ausgelösten Cues um, beginnt seine Überblendung.
    if (pending_ >= 0 && now >= pendingDue_) {
        const int due = pending_;
        pending_ = -1;
        return due;
    }
    return std::nullopt;
}

CueRunnerStatus CueRunner::status(double now) const {
    CueRunnerStatus s;
    s.running = running_;
    if (!running_) {
        return s;
    }
    s.current = current_;
    s.next = nextIndex();
    s.pending = pending_;
    s.pendingSeconds = pending_ >= 0 ? std::max(0.0, pendingDue_ - now) : 0.0;
    if (pending_ < 0 && now < fadeEnd_ && fadeEnd_ > fadeStart_) {
        s.fading = true;
        s.fadeProgress = std::clamp((now - fadeStart_) / (fadeEnd_ - fadeStart_), 0.0, 1.0);
    }
    return s;
}

}  // namespace whitehole::runtime
