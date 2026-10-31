// test_pattern.cpp: rechnet die Testbilder (siehe test_pattern.hpp).
#include "whitehole/runtime/test_pattern.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::runtime {

namespace {

constexpr ColorRgba kOff{0.0F, 0.0F, 0.0F, 1.0F};
constexpr ColorRgba kWhite{1.0F, 1.0F, 1.0F, 1.0F};
/// Breite des Laufbalkens als Anteil der Achse.
constexpr float kSweepWidth = 0.15F;
/// Länge des Schweifs beim Chase (LEDs hinter dem Kopf, mit abnehmender Helligkeit).
constexpr std::uint32_t kChaseTail = 4;

float axisValue(const Vec3& unit, SweepAxis axis) {
    switch (axis) {
        case SweepAxis::X: return unit.x;
        case SweepAxis::Y: return unit.y;
        case SweepAxis::Z: return unit.z;
    }
    return unit.x;
}

float effectiveRate(const TestPattern& p) {
    return (std::isfinite(p.rate) && p.rate > 0.0F) ? p.rate : defaultTestRate(p.kind);
}

bool selected(const TestPattern& p, mapping::FixtureId id) {
    if (p.fixtures.empty()) {
        return p.kind != TestPatternKind::Highlight;  // "nichts gewählt" heißt dort: nichts leuchtet
    }
    return std::find(p.fixtures.begin(), p.fixtures.end(), id) != p.fixtures.end();
}

ColorRgba scaled(float v) { return ColorRgba{v, v, v, 1.0F}; }

}  // namespace

float defaultTestRate(TestPatternKind kind) noexcept {
    switch (kind) {
        case TestPatternKind::Sweep: return 0.4F;
        case TestPatternKind::ColorOrder: return 0.5F;
        case TestPatternKind::Chase: return 12.0F;
        default: return 1.0F;
    }
}

const char* testPatternName(TestPatternKind kind) noexcept {
    switch (kind) {
        case TestPatternKind::Off: return "Aus";
        case TestPatternKind::Color: return "Farbe";
        case TestPatternKind::Highlight: return "Auswahl hervorheben";
        case TestPatternKind::Sweep: return "Laufbalken";
        case TestPatternKind::ColorOrder: return "Farbreihenfolge";
        case TestPatternKind::Chase: return "Lauflicht";
    }
    return "";
}

std::vector<ColorRgba> renderTestPattern(const TestPattern& pattern, const mapping::LedMap& map, double seconds) {
    std::vector<ColorRgba> out(map.size(), kOff);
    if (!(seconds >= 0.0)) {
        seconds = 0.0;  // auch NaN
    }
    if (!pattern.active()) {
        return out;
    }
    const double rate = effectiveRate(pattern);

    // Farbe des Takts bei ColorOrder: Rot, Grün, Blau, Weiß.
    static constexpr ColorRgba kOrder[4] = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 1, 1}};
    const std::uint64_t step = static_cast<std::uint64_t>(std::floor(seconds * rate));
    const double sweepPos = std::fmod(seconds * rate, 1.0);  // 0..1; der Balken läuft von 0 bis 1

    for (const mapping::FixtureRange& range : map.ranges) {
        if (!selected(pattern, range.fixture)) {
            continue;
        }
        const std::size_t end = std::min<std::size_t>(std::size_t{range.first} + range.count, out.size());
        for (std::size_t i = range.first; i < end; ++i) {
            switch (pattern.kind) {
                case TestPatternKind::Color: out[i] = pattern.color; break;
                case TestPatternKind::Highlight: out[i] = kWhite; break;
                case TestPatternKind::ColorOrder: out[i] = kOrder[step % 4]; break;
                case TestPatternKind::Sweep: {
                    const double center = sweepPos;
                    const double d = std::abs(static_cast<double>(axisValue(map.unit[i], pattern.axis)) - center);
                    if (d <= kSweepWidth / 2.0) {
                        out[i] = kWhite;
                    }
                    break;
                }
                case TestPatternKind::Chase: {
                    if (range.count == 0) {
                        break;
                    }
                    const std::uint32_t head = static_cast<std::uint32_t>(step % range.count);
                    const std::uint32_t local = static_cast<std::uint32_t>(i - range.first);
                    // Abstand hinter dem Kopf (mit Umlauf); nur der Kopf und der Schweif leuchten.
                    const std::uint32_t behind = (head + range.count - local) % range.count;
                    if (behind <= kChaseTail && behind < range.count) {
                        out[i] = scaled(1.0F - static_cast<float>(behind) / static_cast<float>(kChaseTail + 1));
                    }
                    break;
                }
                case TestPatternKind::Off: break;
            }
        }
    }
    return out;
}

}  // namespace whitehole::runtime
