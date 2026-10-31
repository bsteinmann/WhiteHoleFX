// analyzer_test.cpp: prüft FFT, Pegel, Spektrum und Beat-Erkennung mit künstlichen Signalen.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <complex>
#include <vector>

#include "signal_support.hpp"
#include "whitehole/audio/analyzer.hpp"
#include "whitehole/audio/fft.hpp"

using namespace whitehole;
using Catch::Approx;

namespace {

/// Index des Bandes mit dem größten Wert.
std::size_t strongestBand(const Spectrum& s) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < s.bands.size(); ++i) {
        if (s.bands[i] > s.bands[best]) {
            best = i;
        }
    }
    return best;
}

/// Band, in das `hz` fällt.
std::size_t bandOf(double hz) {
    const auto& edges = audio::bandEdgesHz();
    for (std::size_t i = 0; i + 1 < edges.size(); ++i) {
        if (hz >= edges[i] && hz < edges[i + 1]) {
            return i;
        }
    }
    return edges.size() - 2;
}

}  // namespace

TEST_CASE("FFT: ein Sinus auf einem Bin erscheint dort mit Amplitude A·N/2", "[audio][fft]") {
    audio::Fft fft{256};
    REQUIRE(fft.size() == 256);
    std::vector<std::complex<float>> data(256);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = {static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979 * 16.0 * static_cast<double>(i) / 256.0)), 0.0F};
    }
    fft.forward(data);
    CHECK(std::abs(data[16]) == Approx(0.5 * 256 / 2).epsilon(0.001));
    CHECK(std::abs(data[17]) < 0.01F);
    CHECK(std::abs(data[0]) < 0.01F);
}

TEST_CASE("FFT: Größe wird zur Zweierpotenz, falsche Datenlänge ändert nichts", "[audio][fft]") {
    audio::Fft fft{100};
    CHECK(fft.size() == 128);
    std::vector<std::complex<float>> wrong(10, {1.0F, 0.0F});
    fft.forward(wrong);
    CHECK(wrong[0] == std::complex<float>{1.0F, 0.0F});
}

TEST_CASE("Bandgrenzen: 33 Werte, logarithmisch von 40 Hz bis 16 kHz", "[audio]") {
    const auto& edges = audio::bandEdgesHz();
    CHECK(edges.front() == Approx(40.0));
    CHECK(edges.back() == Approx(16000.0).epsilon(1e-9));
    for (std::size_t i = 1; i < edges.size(); ++i) {
        CHECK(edges[i] > edges[i - 1]);
    }
}

TEST_CASE("Analyse: Stille liefert Pegel 0 und leeres Spektrum", "[audio]") {
    audio::AudioAnalyzer analyzer;
    test::feed(analyzer, std::vector<float>(48000, 0.0F));
    const auto& frame = analyzer.frame();
    CHECK(frame.frameCount > 50);
    CHECK(frame.rms == 0.0F);
    CHECK(frame.peak == 0.0F);
    for (const float band : frame.spectrum.bands) {
        CHECK(band == 0.0F);
    }
    CHECK(frame.beat.count == 0);
}

TEST_CASE("Analyse: Pegel eines Sinus (Amplitude 0,5: RMS 0,354, Spitze 0,5)", "[audio]") {
    audio::AudioAnalyzer analyzer;
    test::feed(analyzer, test::sine(1000.0, 1.0, 0.5));
    CHECK(analyzer.frame().rms == Approx(0.3536F).margin(0.01));
    CHECK(analyzer.frame().peak == Approx(0.5F).margin(0.01));
}

TEST_CASE("Analyse: ein Sinus erscheint im richtigen Band, Vollaussteuerung ergibt etwa 1", "[audio]") {
    for (const double hz : {100.0, 440.0, 1000.0, 5000.0, 12000.0}) {
        CAPTURE(hz);
        audio::AudioAnalyzer analyzer;
        test::feed(analyzer, test::sine(hz, 1.0));
        const Spectrum& s = analyzer.frame().spectrum;
        const std::size_t expected = bandOf(hz);
        const std::size_t found = strongestBand(s);
        CHECK(found + 1 >= expected);  // höchstens ein Band daneben (Fensterverschmierung, Bandkante)
        CHECK(found <= expected + 1);
        CHECK(s.bands[found] > 0.9F);
    }
}

TEST_CASE("Analyse: ein leiserer Sinus ergibt in dB gerechnet weniger (-20 dB = 1/3 von 60 dB)", "[audio]") {
    audio::AudioAnalyzer loud;
    audio::AudioAnalyzer quiet;
    test::feed(loud, test::sine(1000.0, 1.0, 1.0));
    test::feed(quiet, test::sine(1000.0, 1.0, 0.1));
    const std::size_t band = bandOf(1000.0);
    const float difference = loud.frame().spectrum.bands[band] - quiet.frame().spectrum.bands[band];
    CHECK(difference == Approx(20.0F / 60.0F).margin(0.05));
}

TEST_CASE("Analyse: Eingabe in beliebig kleinen Häppchen gibt dasselbe Ergebnis", "[audio]") {
    const auto samples = test::sine(700.0, 0.5, 0.8);
    audio::AudioAnalyzer a;
    audio::AudioAnalyzer b;
    a.push(samples);
    for (const float s : samples) {
        b.push(std::span<const float>{&s, 1});
    }
    CHECK(a.frame().frameCount == b.frame().frameCount);
    for (std::size_t i = 0; i < kSpectrumBands; ++i) {
        CHECK(a.frame().spectrum.bands[i] == b.frame().spectrum.bands[i]);
    }
}

TEST_CASE("Analyse: reset beginnt von vorn", "[audio]") {
    audio::AudioAnalyzer analyzer;
    test::feed(analyzer, test::sine(440.0, 0.5));
    REQUIRE(analyzer.frame().frameCount > 0);
    analyzer.reset();
    CHECK(analyzer.frame().frameCount == 0);
    CHECK(analyzer.frame().rms == 0.0F);
}

TEST_CASE("Beat: ein Kick-Takt mit 120 BPM wird erkannt, Tempo stimmt", "[audio][beat]") {
    audio::AudioAnalyzer analyzer;
    test::feed(analyzer, test::kickTrack(120.0, 12.0));
    const auto& beat = analyzer.frame().beat;
    // 12 s bei 2 Schlägen pro Sekunde, der erste bei 0,2 s: 23 oder 24 Schläge.
    CHECK(beat.count >= 20);
    CHECK(beat.count <= 26);
    CHECK(beat.bpm == Approx(120.0F).margin(4.0));
    CHECK(beat.strength >= 1.0F);
}

TEST_CASE("Beat: anderes Tempo (90 BPM) wird ebenfalls erkannt", "[audio][beat]") {
    audio::AudioAnalyzer analyzer;
    test::feed(analyzer, test::kickTrack(90.0, 14.0));
    CHECK(analyzer.frame().beat.bpm == Approx(90.0F).margin(4.0));
}

TEST_CASE("Beat: Dauerton und Stille lösen keinen Schlag aus", "[audio][beat]") {
    audio::AudioAnalyzer tone;
    test::feed(tone, test::sine(60.0, 5.0, 0.8));
    CHECK(tone.frame().beat.count <= 1);  // höchstens der Einschwingvorgang am Anfang
    audio::AudioAnalyzer silence;
    test::feed(silence, std::vector<float>(48000 * 3, 0.0F));
    CHECK(silence.frame().beat.count == 0);
    CHECK(silence.frame().beat.bpm == 0.0F);
}

TEST_CASE("Beat: zu wenige Schläge ergeben noch kein Tempo", "[audio][beat]") {
    audio::AudioAnalyzer analyzer;
    test::feed(analyzer, test::kickTrack(120.0, 1.2));
    CHECK(analyzer.frame().beat.bpm == 0.0F);
}
