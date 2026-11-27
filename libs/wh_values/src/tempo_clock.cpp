// tempo_clock.cpp: Umsetzung zu tempo_clock.hpp (gemeinsame Tempo-Uhr für alle Looks).
#include "whitehole/values/tempo_clock.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::values {

namespace {

double clampBpm(double bpm) {
    return std::isfinite(bpm) ? std::clamp(bpm, TempoClock::kMinBpm, TempoClock::kMaxBpm) : TempoClock::kDefaultBpm;
}

}  // namespace

void TempoClock::setSource(Source source) {
    const std::lock_guard lock{mutex_};
    if (source == source_) {
        return;
    }
    source_ = source;
    pendingTaps_ = 0;
    pendingBeats_ = 0;
    lastTap_.reset();
    intervals_.clear();
    if (source_ == Source::Fixed && now_) {
        reanchor(*now_, fixedBpm_);  // fester Wert: das Tempo wechselt, die Phase läuft weiter
    }
}

void TempoClock::setFixedBpm(double bpm) {
    const std::lock_guard lock{mutex_};
    fixedBpm_ = clampBpm(bpm);
    if (source_ == Source::Fixed && now_) {
        reanchor(*now_, fixedBpm_);
    }
}

TempoClock::Source TempoClock::source() const {
    const std::lock_guard lock{mutex_};
    return source_;
}

double TempoClock::fixedBpm() const {
    const std::lock_guard lock{mutex_};
    return fixedBpm_;
}

void TempoClock::tap() {
    const std::lock_guard lock{mutex_};
    if (source_ == Source::Tap) {
        ++pendingTaps_;
    }
}

void TempoClock::externalBeat(double bpm) {
    const std::lock_guard lock{mutex_};
    if (source_ == Source::Audio || source_ == Source::MidiClock) {
        ++pendingBeats_;
        pendingBeatBpm_ = std::isfinite(bpm) && bpm > 0.0 ? bpm : 0.0;
    }
}

void TempoClock::externalBpm(double bpm) {
    const std::lock_guard lock{mutex_};
    if (source_ != Source::Dmx || !std::isfinite(bpm) || bpm <= 0.0 || !now_) {
        return;
    }
    if (std::abs(clampBpm(bpm) - bpm_) > 1e-9) {
        reanchor(*now_, bpm);
    }
}

double TempoClock::targetBeats(double now) const { return anchorBeats_ + (now - anchorTime_) * bpm_ / 60.0; }

double TempoClock::shownBeats(double now) const {
    const double left = std::max(0.0, 1.0 - (now - settleStart_) / kSettleSeconds);
    return targetBeats(now) + settleOffset_ * left;
}

void TempoClock::reanchor(double now, double bpm) {
    // Die gezeigte Phase bleibt stehen, wie sie ist: das noch nicht abgeklungene Angleichen rechnet in den neuen Anker ein
    const double shown = shownBeats(now);
    anchorBeats_ = targetBeats(now);
    anchorTime_ = now;
    bpm_ = clampBpm(bpm);
    settleOffset_ = shown - anchorBeats_;
    settleStart_ = now;
    if (std::abs(settleOffset_) < 1e-12) {
        settleOffset_ = 0.0;
    }
}

void TempoClock::snapTo(double now, double target) {
    const double shown = shownBeats(now);
    anchorBeats_ = target;
    anchorTime_ = now;
    settleOffset_ = shown - target;
    settleStart_ = now;
}

void TempoClock::handleTap(double now) {
    if (lastTap_ && now - *lastTap_ <= kTapTimeout && now > *lastTap_) {
        intervals_.push_back(now - *lastTap_);
        if (intervals_.size() > 4) {
            intervals_.erase(intervals_.begin());
        }
        double sum = 0.0;
        for (const double v : intervals_) {
            sum += v;
        }
        reanchor(now, 60.0 / (sum / static_cast<double>(intervals_.size())));
    } else {
        intervals_.clear();  // erster Tap oder zu lange Pause: das Tempo bleibt, nur die Phase rastet ein
    }
    lastTap_ = now;
    pullBeat(now);  // Tappen zieht die Phase nach: der Tap ist ein Beat
}

void TempoClock::pullBeat(double now) {
    // Gewählt wird die nächste ganze Zahl (kürzester Weg, höchstens ein halber Beat)
    const double beat = std::round(shownBeats(now));
    snapTo(now, beat);
    const auto number = static_cast<std::int64_t>(beat);
    if (number > lastBeat_) {
        lastBeat_ = number;
        lastBeatTime_ = now;
        beatNow_ = true;
    } else if (number == lastBeat_) {
        lastBeatTime_ = now;  // knapp nach dem Beat getappt: kein zweiter Beat, der Puls zählt aber ab dem Tap
    }
}

void TempoClock::update(double now) {
    const std::lock_guard lock{mutex_};
    beatNow_ = false;
    if (!std::isfinite(now)) {
        return;
    }
    if (!now_) {
        now_ = now;
        anchorTime_ = now;
        settleStart_ = now;
        lastBeatTime_ = now;
        bpm_ = source_ == Source::Fixed ? fixedBpm_ : bpm_;
        beatNow_ = true;  // Beat 0 beginnt
    }
    now = std::max(now, *now_);
    now_ = now;
    for (; pendingTaps_ > 0; --pendingTaps_) {
        handleTap(now);
    }
    if (pendingBeats_ > 0) {  // mehrere Schläge seit dem letzten Bild zählen wie einer
        pendingBeats_ = 0;
        if (pendingBeatBpm_ > 0.0) {
            reanchor(now, pendingBeatBpm_);
        }
        pullBeat(now);
    }
    if (settleOffset_ != 0.0 && now - settleStart_ >= kSettleSeconds) {
        settleOffset_ = 0.0;
    }
    const auto current = static_cast<std::int64_t>(std::floor(shownBeats(now)));
    if (current > lastBeat_) {
        lastBeat_ = current;
        lastBeatTime_ = now;
        beatNow_ = true;
    }
}

TempoClock::Snapshot TempoClock::snapshot() const {
    const std::lock_guard lock{mutex_};
    Snapshot s;
    s.bpm = bpm_;
    s.source = source_;
    if (!now_) {
        return s;
    }
    s.beats = shownBeats(*now_);
    s.beat = lastBeat_;
    // Der Bruchteil zählt ab dem gezeigten Beat; während des Angleichens kann die Phase kurz unter dem Beat liegen, dann 0
    s.phase = std::clamp(s.beats - static_cast<double>(lastBeat_), 0.0, 1.0 - 1e-9);
    s.beatNow = beatNow_;
    s.sinceBeat = std::max(0.0, *now_ - lastBeatTime_);
    return s;
}

}  // namespace whitehole::values
