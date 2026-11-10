// live_signals.cpp: Umsetzung zu live_signals.hpp. Verdichtet den ValueStore zu Zahlen, Balken und Farben.
#include "whitehole/runtime/live_signals.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

namespace whitehole::runtime {

namespace {

// Wie schnell der aufgeweitete Bereich zurückfällt (Anteil je Aufruf; bei 25 Aufrufen/s etwa 4 s Halbwertszeit).
constexpr double kRangeDecay = 0.007;

bool numberOf(const graph::ParameterValue& value, double& out) {
    if (const auto* flag = std::get_if<bool>(&value)) {
        out = *flag ? 1.0 : 0.0;
        return true;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        out = static_cast<double>(*integer);
        return true;
    }
    if (const auto* real = std::get_if<double>(&value)) {
        out = *real;
        return std::isfinite(out);
    }
    return false;
}

}  // namespace

std::vector<LiveSignal> collectLiveSignals(const values::ValueStore& store) {
    std::vector<LiveSignal> signals;
    signals.reserve(store.size());
    for (const auto& [node, ports] : store.allValues()) {
        for (const auto& [key, value] : ports) {
            LiveSignal signal;
            signal.node = node;
            signal.portKey = key;
            if (numberOf(value, signal.number)) {
                signal.kind = LiveSignalKind::Number;
            } else if (const auto* color = std::get_if<ColorRgba>(&value)) {
                signal.kind = LiveSignalKind::Color;
                signal.color = *color;
            } else {
                continue;
            }
            signals.push_back(std::move(signal));
        }
    }
    for (const auto& [node, ports] : store.allSpectra()) {
        for (const auto& [key, spectrum] : ports) {
            LiveSignal signal;
            signal.node = node;
            signal.portKey = key;
            signal.kind = LiveSignalKind::Bars;
            signal.bars = spectrum;
            signals.push_back(std::move(signal));
        }
    }
    return signals;
}

float LiveRange::update(double value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    // Bereich langsam auf 0..1 zurückziehen, dann den neuen Wert hineinnehmen.
    low_ += (0.0 - low_) * kRangeDecay;
    high_ += (1.0 - high_) * kRangeDecay;
    low_ = std::min(low_, value);
    high_ = std::max(high_, value);
    const double span = high_ - low_;
    if (span <= 1e-12) {
        return 0.0F;
    }
    return static_cast<float>(std::clamp((value - low_) / span, 0.0, 1.0));
}

}  // namespace whitehole::runtime
