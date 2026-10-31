// utf8.hpp: UTF-8 in Unicode-Codepunkte zerlegen.
//
// Warum es diese Datei gibt: Der Textparameter eines Nodes ist ein UTF-8-String; der Shader rechnet mit
// Codepunkten (u32). Die Umwandlung ist klein und fehlertolerant (ungültige Bytes werden zu U+FFFD), hat
// aber mit dem Feldcompiler nichts zu tun und lässt sich so einzeln prüfen.
// Abhängigkeiten: keine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace whitehole {

inline constexpr std::uint32_t kReplacementCodePoint = 0xFFFD;

/// Zerlegt `text` in Codepunkte, höchstens `maxCount` (der Rest wird abgeschnitten). Ungültige Folgen
/// (abgebrochene Mehrbytezeichen, Überlängen, Ersatzpaare, Werte über U+10FFFF) ergeben je ein U+FFFD
/// und verbrauchen nur das fehlerhafte Byte, damit der Rest nicht verloren geht.
[[nodiscard]] inline std::vector<std::uint32_t> decodeUtf8(std::string_view text, std::size_t maxCount = SIZE_MAX) {
    std::vector<std::uint32_t> out;
    std::size_t i = 0;
    while (i < text.size() && out.size() < maxCount) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        std::uint32_t point = lead;
        std::uint32_t minimum = 0;
        if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            point = lead & 0x07U;
            minimum = 0x10000;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            point = lead & 0x0FU;
            minimum = 0x800;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            point = lead & 0x1FU;
            minimum = 0x80;
        } else if (lead >= 0x80) {
            out.push_back(kReplacementCodePoint);  // Folgebyte ohne Anfang oder unzulässiges Anfangsbyte
            ++i;
            continue;
        }
        bool valid = i + length <= text.size();
        for (std::size_t k = 1; valid && k < length; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            valid = (next & 0xC0U) == 0x80U;
            point = (point << 6) | (next & 0x3FU);
        }
        if (length > 1 && (!valid || point < minimum || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))) {
            out.push_back(kReplacementCodePoint);
            ++i;
            continue;
        }
        out.push_back(point);
        i += length;
    }
    return out;
}

}  // namespace whitehole
