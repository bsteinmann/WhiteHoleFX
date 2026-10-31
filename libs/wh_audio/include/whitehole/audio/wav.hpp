// wav.hpp: liest WAV-Dateien als Mono-Samples.
//
// Warum es diese Datei gibt: Der Audio-Eingang kann statt eines Mikrofons eine WAV-Datei in
// Schleife abspielen, damit sich Effekte ohne Gerät entwickeln und testen lassen. Das Format ist so
// einfach, dass ein eigener Leser weniger Aufwand ist als eine Abhängigkeit.
//
// Unterstützt: PCM mit 8, 16, 24 und 32 Bit sowie 32-Bit-Float, beliebige Kanalzahl (wird zu Mono
// gemittelt), auch WAVE_FORMAT_EXTENSIBLE. Nicht unterstützt: komprimierte Formate (MP3, ADPCM).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::audio {

struct WavData {
    double sampleRate{0.0};
    std::vector<float> mono;  // -1..1
};

/// Fehler: InvalidArgument bei kaputtem oder nicht unterstütztem Inhalt.
[[nodiscard]] Result<WavData> parseWav(std::span<const std::uint8_t> bytes);

/// Fehler zusätzlich: IoError, wenn die Datei nicht lesbar ist, oder zu groß (> 256 MB).
[[nodiscard]] Result<WavData> readWavFile(const std::string& path);

}  // namespace whitehole::audio
