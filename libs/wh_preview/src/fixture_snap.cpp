// fixture_snap.cpp: Umsetzung zu fixture_snap.hpp.
#include "whitehole/preview/fixture_snap.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace whitehole::preview {

namespace {

constexpr float kSame = 1e-4F;  // Linien näher als das gelten als dieselbe Linie

std::array<float, 3> lines(float low, float high) noexcept { return {low, (low + high) * 0.5F, high}; }

/// Kleinste Verschiebung (mit Vorzeichen) von einer Linie des bewegten Rechtecks zu einer Linie eines anderen,
/// höchstens `threshold`; `found` sagt, ob es eine gab.
float bestShift(float low, float high, bool xAxis, std::span<const SnapRect> others, float threshold, bool& found) noexcept {
    float best = 0.0F;
    float bestAbs = threshold + 1.0F;
    for (const SnapRect& other : others) {
        const auto mine = lines(low, high);
        const auto theirs = xAxis ? lines(other.minX, other.maxX) : lines(other.minY, other.maxY);
        for (const float a : mine) {
            for (const float b : theirs) {
                const float d = b - a;
                if (std::abs(d) <= threshold && std::abs(d) < bestAbs) {
                    best = d;
                    bestAbs = std::abs(d);
                }
            }
        }
    }
    found = bestAbs <= threshold;
    return found ? best : 0.0F;
}

}  // namespace

FixtureSnap snapToFixtures(const SnapRect& moving, std::span<const SnapRect> others, float threshold) {
    FixtureSnap result;
    if (others.empty() || threshold <= 0.0F) {
        return result;
    }
    result.dx = bestShift(moving.minX, moving.maxX, true, others, threshold, result.snappedX);
    result.dy = bestShift(moving.minY, moving.maxY, false, others, threshold, result.snappedY);
    const SnapRect at{moving.minX + result.dx, moving.minY + result.dy, moving.maxX + result.dx, moving.maxY + result.dy};
    for (const SnapRect& other : others) {
        if (result.snappedX) {
            for (const float a : lines(at.minX, at.maxX)) {
                for (const float b : lines(other.minX, other.maxX)) {
                    if (std::abs(a - b) < kSame) {
                        result.guides.push_back({true, b, std::min(at.minY, other.minY), std::max(at.maxY, other.maxY)});
                    }
                }
            }
        }
        if (result.snappedY) {
            for (const float a : lines(at.minY, at.maxY)) {
                for (const float b : lines(other.minY, other.maxY)) {
                    if (std::abs(a - b) < kSame) {
                        result.guides.push_back({false, b, std::min(at.minX, other.minX), std::max(at.maxX, other.maxX)});
                    }
                }
            }
        }
    }
    return result;
}

}  // namespace whitehole::preview
