// look_thumbnail.hpp: aus einem Vorschaubild ein winziges Standbild für den Look-Knopf machen.
//
// Warum es diese Datei gibt: Beim Speichern eines Looks wird die aktuelle Ausgabe als kleines Bild festgehalten, damit
// man im Live-Fenster sieht, was ein Knopf auslöst. Das Verkleinern ist reine Rechnerei und steht hier getrennt vom
// Player, damit man es ohne Projekt testen kann.
#pragma once

#include <cstdint>
#include <optional>

#include "whitehole/preview/preview.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

inline constexpr std::uint32_t kThumbnailWidth = 32;
inline constexpr std::uint32_t kThumbnailHeight = 18;

/// Verkleinert `image` (RGBA) per Mittelwert auf `width` x `height` (RGB, Alpha über Schwarz gelegt).
/// Leer, wenn das Bild leer oder kleiner als das Ziel ist oder die Maße ungültig sind.
[[nodiscard]] std::optional<project::LookThumbnail> makeLookThumbnail(const preview::Image& image,
                                                                      std::uint32_t width = kThumbnailWidth,
                                                                      std::uint32_t height = kThumbnailHeight);

}  // namespace whitehole::runtime
