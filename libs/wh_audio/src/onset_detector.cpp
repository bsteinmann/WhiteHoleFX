// onset_detector.cpp: Umsetzung zu onset_detector.hpp.
#include "whitehole/audio/onset_detector.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::audio {

namespace {

constexpr double kHistorySeconds = 1.0;  // Fenster der adaptiven Schwelle
constexpr double kSigma = 1.5;           // Schwelle = Mittelwert + 1,5 · Streuung
constexpr double kMinFlux = 0.03;        // mittlerer Anstieg je Band (0..1 entspricht 60 dB): darunter Rauschen

}  // namespace

OnsetDetector::OnsetDetector(double hopSeconds, std::size_t firstBand, std::size_t endBand, double minInterval)
    : hopSeconds_{hopSeconds > 0.0 ? hopSeconds : 0.01},
      first_{std::min(firstBand, kSpectrumBands)},
      end_{std::clamp(endBand, std::min(firstBand, kSpectrumBands), kSpectrumBands)},
      minInterval_{minInterval} {}

void OnsetDetector::reset() {
    hasPrevious_ = false;
    previousFlux_ = 0.0;
    history_.clear();
    info_ = {};
}

void OnsetDetector::push(const Spectrum& spectrum, double time) {
    double flux = 0.0;
    if (hasPrevious_ && end_ > first_) {
        for (std::size_t b = first_; b < end_; ++b) {
            flux += std::max(0.0F, spectrum.bands[b] - previous_.bands[b]);
        }
        flux /= static_cast<double>(end_ - first_);  // Mittel: unabhängig von der Zahl der Bänder
    }
    previous_ = spectrum;
    hasPrevious_ = true;

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
    const double threshold = std::max(kMinFlux, mean + kSigma * deviation);

    const bool rising = flux > previousFlux_;
    const bool spaced = info_.count == 0 || time - info_.lastTime >= minInterval_;
    if (flux > threshold && rising && spaced && time > 0.0) {
        info_.count += 1;
        info_.strength = static_cast<float>(flux / threshold);
        info_.lastTime = time;
    }
    previousFlux_ = flux;

    history_.push_back(flux);
    const auto limit = static_cast<std::size_t>(kHistorySeconds / hopSeconds_);
    while (history_.size() > std::max<std::size_t>(limit, 8)) {
        history_.pop_front();
    }
}

}  // namespace whitehole::audio
