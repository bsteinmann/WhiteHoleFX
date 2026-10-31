// wav.cpp: Umsetzung zu wav.hpp.
#include "whitehole/audio/wav.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace whitehole::audio {

namespace {

constexpr std::size_t kMaxFileBytes = 256U * 1024U * 1024U;

Error bad(const std::string& message) { return Error{ErrorCode::InvalidArgument, "WAV: " + message}; }

std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) | (static_cast<std::uint32_t>(p[3]) << 24U);
}
std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8U));
}

/// Ein Sample aus `bytesPerSample` Bytes (little endian) als -1..1.
float decode(const std::uint8_t* p, unsigned bytesPerSample, bool isFloat) {
    if (isFloat) {
        float value = 0.0F;
        std::memcpy(&value, p, sizeof(float));
        return value;
    }
    switch (bytesPerSample) {
        case 1:
            return (static_cast<float>(p[0]) - 128.0F) / 128.0F;
        case 2:
            return static_cast<float>(static_cast<std::int16_t>(le16(p))) / 32768.0F;
        case 3: {
            std::int32_t v = static_cast<std::int32_t>((static_cast<std::uint32_t>(p[0]) << 8U) |
                                                       (static_cast<std::uint32_t>(p[1]) << 16U) |
                                                       (static_cast<std::uint32_t>(p[2]) << 24U));
            return static_cast<float>(v / 256) / 8388608.0F;
        }
        default:
            return static_cast<float>(static_cast<double>(static_cast<std::int32_t>(le32(p))) / 2147483648.0);
    }
}

}  // namespace

Result<WavData> parseWav(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return bad("keine RIFF/WAVE-Datei");
    }
    unsigned format = 0;
    unsigned channels = 0;
    unsigned bits = 0;
    std::uint32_t rate = 0;
    const std::uint8_t* data = nullptr;
    std::size_t dataSize = 0;

    std::size_t pos = 12;
    while (pos + 8 <= bytes.size()) {
        const std::uint8_t* chunk = bytes.data() + pos;
        std::size_t size = le32(chunk + 4);
        const std::size_t bodyStart = pos + 8;
        if (bodyStart + size > bytes.size()) {
            size = bytes.size() - bodyStart;  // abgeschnittene Datei: nimm, was da ist
        }
        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
            const std::uint8_t* f = bytes.data() + bodyStart;
            format = le16(f);
            channels = le16(f + 2);
            rate = le32(f + 4);
            bits = le16(f + 14);
            if (format == 0xFFFE && size >= 26) {
                format = le16(f + 24);  // Unterformat der Erweiterung
            }
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            data = bytes.data() + bodyStart;
            dataSize = size;
            break;
        }
        pos = bodyStart + size + (size & 1U);  // Chunks sind auf gerade Längen ausgerichtet
    }
    if (rate == 0 || channels == 0) {
        return bad("fmt-Abschnitt fehlt");
    }
    if (data == nullptr) {
        return bad("data-Abschnitt fehlt");
    }
    const bool isFloat = format == 3;
    if (!(format == 1 || isFloat)) {
        return bad("nur PCM und Float werden unterstützt (Format " + std::to_string(format) + ")");
    }
    if (isFloat ? bits != 32 : !(bits == 8 || bits == 16 || bits == 24 || bits == 32)) {
        return bad(std::to_string(bits) + " Bit werden nicht unterstützt");
    }

    const unsigned bytesPerSample = bits / 8U;
    const std::size_t frameBytes = static_cast<std::size_t>(bytesPerSample) * channels;
    const std::size_t frames = dataSize / frameBytes;

    WavData out;
    out.sampleRate = static_cast<double>(rate);
    out.mono.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        float sum = 0.0F;
        for (unsigned c = 0; c < channels; ++c) {
            sum += decode(data + i * frameBytes + static_cast<std::size_t>(c) * bytesPerSample, bytesPerSample, isFloat);
        }
        out.mono[i] = sum / static_cast<float>(channels);
    }
    return out;
}

Result<WavData> readWavFile(const std::string& path) {
    // Pfade sind UTF-8; der Umweg über char8_t verhindert, dass Windows die Ansi-Codepage annimmt.
    const std::filesystem::path fsPath{std::u8string{reinterpret_cast<const char8_t*>(path.data()), path.size()}};
    std::ifstream file{fsPath, std::ios::binary | std::ios::ate};
    if (!file) {
        return Error{ErrorCode::IoError, "WAV-Datei nicht lesbar: " + path};
    }
    const auto size = static_cast<std::size_t>(file.tellg());
    if (size > kMaxFileBytes) {
        return Error{ErrorCode::IoError, "WAV-Datei zu groß (über 256 MB): " + path};
    }
    std::vector<std::uint8_t> bytes(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!file) {
        return Error{ErrorCode::IoError, "WAV-Datei nicht lesbar: " + path};
    }
    return parseWav(bytes);
}

}  // namespace whitehole::audio
