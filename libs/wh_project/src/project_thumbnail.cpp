// project_thumbnail.cpp: Hex-Kodierung des Look-Standbilds (siehe project_thumbnail.hpp).
//
// Warum es diese Datei gibt: Lesen und Schreiben stehen nebeneinander, damit Format und Prüfungen zusammenpassen.
#include "project_thumbnail.hpp"

#include <cstddef>

namespace whitehole::project::detail {

namespace {

int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

}  // namespace

LookThumbnail parseThumbnail(Reader& r) {
    LookThumbnail thumbnail;
    thumbnail.width = r.whole("Bildbreite", kMaxThumbnailSide);
    thumbnail.height = r.whole("Bildhöhe", kMaxThumbnailSide);
    const std::string hex = r.word("Bilddaten");
    r.expectEnd();
    if (!r.ok()) {
        return {};
    }
    if (thumbnail.width == 0 || thumbnail.height == 0) {
        r.fail("Das Standbild braucht Breite und Höhe von mindestens 1");
        return {};
    }
    const std::size_t bytes = static_cast<std::size_t>(thumbnail.width) * thumbnail.height * 3;
    if (hex.size() != bytes * 2) {
        r.fail("Die Bilddaten des Standbilds haben die falsche Länge");
        return {};
    }
    thumbnail.rgb.resize(bytes);
    for (std::size_t i = 0; i < bytes; ++i) {
        const int high = hexDigit(hex[i * 2]);
        const int low = hexDigit(hex[i * 2 + 1]);
        if (high < 0 || low < 0) {
            r.fail("Die Bilddaten des Standbilds sind kein Hex-Text");
            return {};
        }
        thumbnail.rgb[i] = static_cast<std::uint8_t>(high * 16 + low);
    }
    return thumbnail;
}

std::string formatThumbnail(const LookThumbnail& thumbnail) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string line = "  thumb " + std::to_string(thumbnail.width) + " " + std::to_string(thumbnail.height) + " ";
    line.reserve(line.size() + thumbnail.rgb.size() * 2 + 1);
    for (const std::uint8_t byte : thumbnail.rgb) {
        line += kDigits[byte >> 4];
        line += kDigits[byte & 0x0F];
    }
    line += "\n";
    return line;
}

}  // namespace whitehole::project::detail
