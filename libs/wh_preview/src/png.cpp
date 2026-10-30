// png.cpp: PNG mit unkomprimierten Deflate-Blöcken.
#include "whitehole/preview/png.hpp"

#include <algorithm>
#include <array>
#include <fstream>

namespace whitehole::preview {

namespace {

constexpr std::size_t kMaxStoredBlock = 65535;

std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1U) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
        }
        table[n] = c;
    }
    return table;
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void putChunk(std::vector<std::uint8_t>& out, const char (&type)[5], std::span<const std::uint8_t> data) {
    putU32(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    putU32(out, crc32(body));
}

}  // namespace

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept {
    static const std::array<std::uint32_t, 256> table = makeCrcTable();
    std::uint32_t c = 0xFFFFFFFFU;
    for (const std::uint8_t byte : data) {
        c = table[(c ^ byte) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

std::uint32_t adler32(std::span<const std::uint8_t> data) noexcept {
    constexpr std::uint32_t kMod = 65521;
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t byte : data) {
        a = (a + byte) % kMod;
        b = (b + a) % kMod;
    }
    return (b << 16) | a;
}

Result<std::vector<std::uint8_t>> encodePng(const Image& image) {
    if (image.width == 0 || image.height == 0) {
        return Error{ErrorCode::InvalidArgument, "Bildgröße 0"};
    }
    const std::uint64_t expected = std::uint64_t{image.width} * image.height * 4;
    if (image.rgba.size() != expected) {
        return Error{ErrorCode::InvalidArgument, "Die Pixeldaten passen nicht zur Bildgröße"};
    }

    // Rohdaten: je Zeile ein Filterbyte 0 (keiner), dann die Pixel.
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(expected) + image.height);
    const std::size_t rowBytes = std::size_t{image.width} * 4;
    for (std::uint32_t y = 0; y < image.height; ++y) {
        raw.push_back(0);
        const auto first = image.rgba.begin() + static_cast<std::ptrdiff_t>(y * rowBytes);
        raw.insert(raw.end(), first, first + static_cast<std::ptrdiff_t>(rowBytes));
    }

    // zlib: Kopf, unkomprimierte Deflate-Blöcke, Adler-32.
    std::vector<std::uint8_t> z{0x78, 0x01};
    std::size_t pos = 0;
    do {
        const std::size_t len = std::min(kMaxStoredBlock, raw.size() - pos);
        const bool last = pos + len >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(len & 0xFFU));
        z.push_back(static_cast<std::uint8_t>(len >> 8));
        z.push_back(static_cast<std::uint8_t>(~len & 0xFFU));
        z.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFFU));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    } while (pos < raw.size());
    putU32(z, adler32(raw));

    std::vector<std::uint8_t> out{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> header;
    putU32(header, image.width);
    putU32(header, image.height);
    header.insert(header.end(), {8, 6, 0, 0, 0});  // 8 Bit, RGBA, Standardverfahren, kein Interlace
    putChunk(out, "IHDR", header);
    putChunk(out, "IDAT", z);
    putChunk(out, "IEND", {});
    return out;
}

Result<void> writePng(const Image& image, const std::string& path) {
    const auto bytes = encodePng(image);
    if (!bytes.ok()) {
        return bytes.error();
    }
    std::ofstream out{path, std::ios::binary};
    if (!out) {
        return Error{ErrorCode::IoError, "Datei kann nicht geschrieben werden: " + path};
    }
    out.write(reinterpret_cast<const char*>(bytes.value().data()),
              static_cast<std::streamsize>(bytes.value().size()));
    out.close();
    if (!out) {
        return Error{ErrorCode::IoError, "Schreibfehler: " + path};
    }
    return {};
}

}  // namespace whitehole::preview
