// onset_test.cpp: Einsatz-Erkennung je Frequenzbereich (Kick, Snare, Hi-Hat) mit künstlichen Signalen.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "signal_support.hpp"

using namespace whitehole;
using whitehole::test::feed;
using whitehole::test::kickTrack;

namespace {

/// Kurze Töne (30 ms, abklingend) von `hz` im Abstand `period`, erster bei 0,2 s.
std::vector<float> bursts(double hz, double period, double seconds, double amplitude, double rate = 48000.0) {
    std::vector<float> out(static_cast<std::size_t>(seconds * rate), 0.0F);
    for (double t = 0.2; t < seconds; t += period) {
        const auto start = static_cast<std::size_t>(t * rate);
        const auto length = static_cast<std::size_t>(0.03 * rate);
        for (std::size_t i = 0; i < length && start + i < out.size(); ++i) {
            const double x = static_cast<double>(i) / rate;
            out[start + i] = static_cast<float>(amplitude * std::exp(-x * 60.0) * std::sin(2.0 * std::numbers::pi * hz * x));
        }
    }
    return out;
}

}  // namespace

TEST_CASE("Einsatz: ein Kick-Takt löst nur den Kick-Erkenner aus", "[audio][onset]") {
    audio::AudioAnalyzer analyzer;
    feed(analyzer, kickTrack(120.0, 12.0));  // 23 bis 24 Kicks
    const auto& frame = analyzer.frame();
    CHECK(frame.kick.count >= 20);
    CHECK(frame.kick.count <= 26);
    CHECK(frame.kick.strength >= 1.0F);
    CHECK(frame.hat.count == 0);
}

TEST_CASE("Einsatz: kurze hohe Töne lösen nur den Hi-Hat-Erkenner aus", "[audio][onset]") {
    audio::AudioAnalyzer analyzer;
    feed(analyzer, bursts(8000.0, 0.25, 8.0, 0.5));  // 31 Töne
    const auto& frame = analyzer.frame();
    CHECK(frame.hat.count >= 26);
    CHECK(frame.hat.count <= 34);
    CHECK(frame.kick.count == 0);
}

TEST_CASE("Einsatz: auch ein sehr leiser Kick wird erkannt (Schwelle folgt dem Pegel)", "[audio][onset]") {
    std::vector<float> quiet = kickTrack(120.0, 12.0);
    for (float& s : quiet) {
        s *= 0.02F;  // etwa -35 dB
    }
    audio::AudioAnalyzer analyzer;
    feed(analyzer, quiet);
    CHECK(analyzer.frame().kick.count >= 18);
}

TEST_CASE("Einsatz: der erste Kick wird früh gemeldet (höchstens 60 ms nach dem Einsatz)", "[audio][onset]") {
    audio::AudioAnalyzer analyzer;
    const std::vector<float> samples = kickTrack(120.0, 2.0);  // erster Kick bei 0,2 s
    constexpr std::size_t kChunk = 441;
    double detected = -1.0;
    for (std::size_t i = 0; i < samples.size() && detected < 0.0; i += kChunk) {
        analyzer.push(std::span<const float>{samples.data() + i, std::min(kChunk, samples.size() - i)});
        if (analyzer.frame().kick.count > 0) {
            detected = analyzer.frame().audioTime;
        }
    }
    REQUIRE(detected > 0.0);
    CHECK(detected - 0.2 < 0.06);
    CHECK(detected - 0.2 >= 0.0);
}

TEST_CASE("Einsatz: Stille und Dauerton lösen nichts aus, reset zählt von vorn", "[audio][onset]") {
    audio::AudioAnalyzer silence;
    feed(silence, std::vector<float>(48000 * 3, 0.0F));
    CHECK(silence.frame().kick.count == 0);
    CHECK(silence.frame().snare.count == 0);
    CHECK(silence.frame().hat.count == 0);

    audio::AudioAnalyzer tone;
    feed(tone, test::sine(1000.0, 3.0, 0.5));
    CHECK(tone.frame().kick.count == 0);
    CHECK(tone.frame().hat.count == 0);

    audio::AudioAnalyzer kicks;
    feed(kicks, kickTrack(120.0, 4.0));
    REQUIRE(kicks.frame().kick.count > 0);
    kicks.reset();
    CHECK(kicks.frame().kick.count == 0);
}
