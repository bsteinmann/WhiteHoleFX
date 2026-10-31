// recent_files.hpp: die Liste "Zuletzt geöffnet".
//
// Warum es das gibt: Wer täglich an denselben Projekten arbeitet, will sie mit einem Klick öffnen.
// Das Studio speichert die Liste (QSettings); wie sie sich beim Öffnen verändert, ist reine
// Listenlogik und steht hier ohne Qt, damit sie testbar ist.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace whitehole::runtime {

inline constexpr std::size_t kMaxRecentFiles = 10;

/// Pfad in eine vergleichbare Form bringen (absolut, `.`/`..` aufgelöst). Der Pfad muss nicht existieren.
[[nodiscard]] std::string normalizedPath(const std::string& path);

/// Setzt `path` an den Anfang; war er schon in der Liste (auch in anderer Schreibweise, z. B. `a/../b`),
/// rutscht er nur nach vorn. Die Liste wird auf `max` Einträge gekürzt. Ein leerer Pfad ändert nichts.
[[nodiscard]] std::vector<std::string> addRecentFile(std::vector<std::string> list, const std::string& path,
                                                     std::size_t max = kMaxRecentFiles);

/// Entfernt `path` (auch in anderer Schreibweise) aus der Liste.
[[nodiscard]] std::vector<std::string> removeRecentFile(std::vector<std::string> list, const std::string& path);

}  // namespace whitehole::runtime
