// beat_detector.cpp: Umsetzung zu beat_detector.hpp.
#include "whitehole/audio/beat_detector.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace whitehole::audio {

namespace {

constexpr std::size_t kFluxBands = 16;       // Bänder 0..15: bis etwa 1,5 kHz, dort sitzen Kick und Snare
constexpr double kHistorySeconds = 1.0;      // Länge des Fensters für die adaptive Schwelle
constexpr double kThresholdSigma = 1.2;      // Schwelle = Mittelwert + 1,2 · Streuung
constexpr double kMinFlux = 0.25;            // darunter ist es Rauschen (Summe über 16 Bänder, je 0..1)
constexpr std::size_t kTempoBeats = 12;      // so viele Schlagabstände gehen in den Median ein
constexpr double kBpmLow = 70.0;
constexpr double kBpmHigh = 160.0;

}  // namespace

BeatDetector::BeatDetector(double hopSeconds, double minInterval)
    : hopSeconds_{hopSeconds > 0.0 ? hopSeconds : 0.01}, minInterval_{minInterval} {}

void BeatDetector::reset() {
    hasPrevious_ = false;
    recent_ = {};
    recentTime_ = {};
    history_.clear();
    beatTimes_.clear();
    info_ = {};
}

void BeatDetector::push(const Spectrum& spectrum, double time) {
    double flux = 0.0;
    if (hasPrevious_) {
        for (std::size_t b = 0; b < kFluxBands; ++b) {
            flux += std::max(0.0F, spectrum.bands[b] - previous_.bands[b]);
        }
    }
    previous_ = spectrum;
    hasPrevious_ = true;

    // Schwelle aus der Vergangenheit (ohne den aktuellen Wert).
    double mean = 0.0;
    double deviation = 0.0;
    if (!history_.empty()) {
        for (const double v : history_) {
            mean += v;
        }
        mean /= static_cast<double>(history_.size());
        for (const double v : history_) {
            deviation += (v - mean) * (v - mean);
        }
        deviation = std::sqrt(deviation / static_cast<double>(history_.size()));
    }
    const double threshold = std::max(kMinFlux, mean + kThresholdSigma * deviation);

    recent_ = {recent_[1], recent_[2], flux};
    recentTime_ = {recentTime_[1], recentTime_[2], time};

    // Der mittlere Wert ist der Kandidat: Er muss größer als der vorige und nicht kleiner als der
    // nächste sein (lokales Maximum), damit ein Schlag einmal gezählt wird und nicht mehrfach.
    const double candidate = recent_[1];
    const bool peak = candidate > recent_[0] && candidate >= recent_[2];
    const bool spaced = beatTimes_.empty() || recentTime_[1] - beatTimes_.back() >= minInterval_;
    if (peak && candidate > threshold && spaced && recentTime_[1] > 0.0) {
        info_.count += 1;
        info_.strength = static_cast<float>(candidate / threshold);
        info_.lastBeatTime = recentTime_[1];
        beatTimes_.push_back(recentTime_[1]);
        while (beatTimes_.size() > kTempoBeats + 1) {
            beatTimes_.pop_front();
        }
        updateTempo();
    }

    history_.push_back(flux);
    const auto limit = static_cast<std::size_t>(kHistorySeconds / hopSeconds_);
    while (history_.size() > std::max<std::size_t>(limit, 8)) {
        history_.pop_front();
    }
}

void BeatDetector::updateTempo() {
    if (beatTimes_.size() < 5) {
        return;  // vier Abstände mindestens
    }
    std::vector<double> bpms;
    for (std::size_t i = 1; i < beatTimes_.size(); ++i) {
        const double interval = beatTimes_[i] - beatTimes_[i - 1];
        if (interval <= 0.0) {
            continue;
        }
        double bpm = 60.0 / interval;
        while (bpm < kBpmLow) {
            bpm *= 2.0;
        }
        while (bpm >= kBpmHigh) {
            bpm /= 2.0;
        }
        bpms.push_back(bpm);
    }
    if (bpms.empty()) {
        return;
    }
    std::sort(bpms.begin(), bpms.end());
    info_.bpm = static_cast<float>(bpms[bpms.size() / 2]);
}

}  // namespace whitehole::audio
