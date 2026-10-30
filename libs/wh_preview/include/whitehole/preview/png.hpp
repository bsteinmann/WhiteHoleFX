// png.hpp: schreibt ein Bild als PNG.
//
// Warum es diese Datei gibt: Eine Bilddatei ist der einfachste Weg, eine Vorschau zu teilen oder
// in einen Fehlerbericht zu hängen. Eine Kompressionsbibliothek wäre eine Abhängigkeit mehr;
// deshalb schreibt dieser Schreiber unkomprimierte Deflate-Blöcke. Die Dateien sind größer, aber
// jeder PNG-Leser öffnet sie. Für Vorschauen mit einigen hundert Kilobyte ist das unerheblich.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/preview/preview.hpp"

namespace whitehole::preview {

/// Fehler: InvalidArgument, wenn Breite oder Höhe 0 ist oder `rgba` nicht width*height*4 Bytes hat.
[[nodiscard]] Result<std::vector<std::uint8_t>> encodePng(const Image& image);

/// Schreibt die Datei. Fehler: wie encodePng, IoError.
[[nodiscard]] Result<void> writePng(const Image& image, const std::string& path);

/// Prüfsummen, öffentlich für Tests.
[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] std::uint32_t adler32(std::span<const std::uint8_t> data) noexcept;

}  // namespace whitehole::preview
