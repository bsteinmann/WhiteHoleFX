// signal_support.hpp: künstliche Audiosignale für die Tests der Analyse.
//
// Warum es diese Datei gibt: Die Analyse (wh_audio) wird ohne Audiogerät geprüft. Alle Tests
// brauchen dieselben Bausteine: einen Sinus, Stille und einen "Kick-Takt" mit bekanntem Tempo.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include "whitehole/audio/analyzer.hpp"

namespace whitehole::test {

inline std::vector<float> sine(double hz, double seconds, double amplitude = 1.0, double rate = 48000.0) {
    std::vector<float> out(static_cast<std::size_t>(seconds * rate));
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / rate));
    }
    return out;
}

/// Ein "Kick"-Takt: alle 60/bpm Sekunden ein abklingender 60-Hz-Ton (Dauer ~0,15 s), dazwischen Stille.
inline std::vector<float> kickTrack(double bpm, double seconds, double rate = 48000.0) {
    std::vector<float> out(static_cast<std::size_t>(seconds * rate), 0.0F);
    const double period = 60.0 / bpm;
    for (double t = 0.2; t < seconds; t += period) {
        const auto start = static_cast<std::size_t>(t * rate);
        const auto length = static_cast<std::size_t>(0.15 * rate);
        for (std::size_t i = 0; i < length && start + i < out.size(); ++i) {
            const double x = static_cast<double>(i) / rate;
            out[start + i] = static_cast<float>(0.9 * std::exp(-x * 25.0) * std::sin(2.0 * std::numbers::pi * 60.0 * x));
        }
    }
    return out;
}

/// Füttert den Analysator in Häppchen von 441 Samples (wie ein Gerät, das 10 ms liefert).
inline void feed(audio::AudioAnalyzer& analyzer, const std::vector<float>& samples) {
    constexpr std::size_t kChunk = 441;
    for (std::size_t i = 0; i < samples.size(); i += kChunk) {
        const std::size_t n = std::min(kChunk, samples.size() - i);
        analyzer.push(std::span<const float>{samples.data() + i, n});
    }
}

/// Packt Mono-Samples als 16-Bit-PCM-WAV (Mono oder, mit `stereo`, zwei gleiche Kanäle).
inline std::vector<std::uint8_t> wavBytes(const std::vector<float>& samples, std::uint32_t rate = 48000, bool stereo = false) {
    const std::uint16_t channels = stereo ? 2 : 1;
    const auto dataBytes = static_cast<std::uint32_t>(samples.size() * channels * 2);
    std::vector<std::uint8_t> out;
    const auto put32 = [&out](std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    const auto put16 = [&out](std::uint16_t v) { out.push_back(static_cast<std::uint8_t>(v)); out.push_back(static_cast<std::uint8_t>(v >> 8)); };
    const auto tag = [&out](const char* t) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(t[i])); };
    tag("RIFF"); put32(36 + dataBytes); tag("WAVE");
    tag("fmt "); put32(16); put16(1); put16(channels); put32(rate); put32(rate * channels * 2); put16(static_cast<std::uint16_t>(channels * 2)); put16(16);
    tag("data"); put32(dataBytes);
    for (const float s : samples) {
        const auto v = static_cast<std::int16_t>(std::lround(std::clamp(s, -1.0F, 1.0F) * 32767.0F));
        for (int c = 0; c < channels; ++c) put16(static_cast<std::uint16_t>(v));
    }
    return out;
}

}  // namespace whitehole::test
