// hash.hpp: FNV-1a (64 Bit) für Strukturhashes, z. B. als Schlüssel für Pipeline-Caches.
#pragma once

#include <cstdint>
#include <string_view>

namespace whitehole {

/// Nicht kryptografisch. Gleiche Eingabe ergibt auf jeder Plattform denselben Wert.
[[nodiscard]] constexpr std::uint64_t fnv1a64(std::string_view data) noexcept {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    std::uint64_t hash = kOffsetBasis;
    for (const char c : data) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= kPrime;
    }
    return hash;
}

}  // namespace whitehole
