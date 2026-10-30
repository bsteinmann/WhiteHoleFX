// frame_clocks.hpp: Implementierungen von IFrameClock.
// Beide Uhren messen nur; das Takten auf eine feste Rate (Schlafen bis zum
// nächsten Frame) übernimmt der Frame-Loop.
#pragma once

#include <chrono>

#include "whitehole/core/services.hpp"

namespace whitehole::engine {

/// Deterministische Uhr: Jeder advance() schreibt die Zeit um genau `step` fort.
/// Für Tests und Offline-Rendering (Bake). Die Zeit wird aus dem Frame-Index
/// berechnet statt aufaddiert, damit sie auch nach vielen Frames nicht driftet.
class ManualFrameClock final : public IFrameClock {
public:
    explicit ManualFrameClock(Seconds step) noexcept : step_{step} {}

    [[nodiscard]] FrameTime current() const override { return current_; }

    FrameTime advance() override {
        ++current_.frame;
        current_.time = step_ * static_cast<double>(current_.frame);
        current_.delta = step_;
        return current_;
    }

private:
    Seconds step_;
    FrameTime current_{};
};

/// Echtzeit-Uhr auf Basis von std::chrono::steady_clock. Die Zeit läuft ab dem
/// Konstruktor; sie ist monoton und von Systemzeit-Änderungen unabhängig.
class SteadyFrameClock final : public IFrameClock {
public:
    SteadyFrameClock() noexcept : origin_{Clock::now()} {}

    [[nodiscard]] FrameTime current() const override { return current_; }

    FrameTime advance() override {
        const Seconds now = Clock::now() - origin_;
        current_.delta = now - current_.time;
        current_.time = now;
        ++current_.frame;
        return current_;
    }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point origin_;
    FrameTime current_{};
};

}  // namespace whitehole::engine
