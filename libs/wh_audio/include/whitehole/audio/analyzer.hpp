// analyzer.hpp: macht aus Audio-Samples Pegel, 32-Band-Spektrum und Beat-Daten.
//
// Warum es diese Datei gibt: Zwischen Aufnahmegerät (oder WAV-Datei) und den Nodes steht genau
// eine Stelle, die rechnet. Sie bekommt Mono-Samples in beliebig großen Häppchen und liefert nach
// jedem vollen Schritt (Hop) einen neuen `AudioFrame`. Sie kennt kein Gerät und keinen Thread und
// ist daher mit künstlichen Signalen genau testbar.
//
// Skalen (damit Nodes ohne Umrechnung arbeiten können):
//   - rms/peak: linear, 0..1 (Vollaussteuerung = 1).
//   - Spektrum: Amplitude je Band in dBFS, abgebildet von -60 dB (= 0) bis 0 dB (= 1). Ein
//     Vollaussteuerungs-Sinus liefert in seinem Band 1.
// Bänder: 32, logarithmisch von 40 Hz bis 16 kHz (`bandEdgesHz`).
#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "whitehole/audio/beat_detector.hpp"
#include "whitehole/audio/fft.hpp"
#include "whitehole/core/spectrum.hpp"

namespace whitehole::audio {

inline constexpr double kBandLowHz = 40.0;
inline constexpr double kBandHighHz = 16000.0;
inline constexpr double kFloorDb = -60.0;

/// Die 33 Bandgrenzen in Hz (Band i reicht von edges[i] bis edges[i+1]).
[[nodiscard]] const std::array<double, kSpectrumBands + 1>& bandEdgesHz();

/// Ergebnis eines Analyseschritts.
struct AudioFrame {
    float rms{0.0F};
    float peak{0.0F};
    Spectrum spectrum{};
    BeatInfo beat{};
    double audioTime{0.0};       // Sekunden seit Start der Analyse (aus der Zahl der Samples)
    std::uint64_t frameCount{0}; // Zahl der bisher berechneten Schritte
};

struct AnalyzerConfig {
    double sampleRate{48000.0};
    std::size_t fftSize{2048};   // Fenster; wird zur Zweierpotenz
    std::size_t hop{512};        // neue Samples je Schritt
    double minBeatInterval{0.3};
};

class AudioAnalyzer {
public:
    explicit AudioAnalyzer(AnalyzerConfig config = {});

    /// Hängt Mono-Samples an. Gibt die Zahl der neuen Schritte zurück; `frame()` ist der letzte.
    std::size_t push(std::span<const float> mono);

    [[nodiscard]] const AudioFrame& frame() const noexcept { return frame_; }
    [[nodiscard]] const AnalyzerConfig& config() const noexcept { return config_; }
    void reset();

private:
    void step();

    AnalyzerConfig config_;
    Fft fft_;
    BeatDetector beat_;
    std::vector<float> window_;          // Hann-Fenster
    double windowGain_{1.0};             // Summe des Fensters / 2: Normierung auf Amplitude
    std::vector<float> ring_;            // die letzten fftSize Samples
    std::size_t writePos_{0};
    std::size_t filled_{0};
    std::size_t sinceStep_{0};
    std::uint64_t totalSamples_{0};
    float hopPeak_{0.0F};
    std::vector<std::complex<float>> scratch_;
    std::array<std::size_t, kSpectrumBands> firstBin_{};
    std::array<std::size_t, kSpectrumBands> lastBin_{};  // einschließlich
    AudioFrame frame_;
};

}  // namespace whitehole::audio
