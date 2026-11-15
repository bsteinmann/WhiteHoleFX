// live_dmx.cpp: Umsetzung zu live_dmx.hpp.
#include <algorithm>

#include "whitehole/runtime/live_dmx.hpp"

namespace whitehole::runtime {

std::optional<int> lookIndexFromDmxValue(std::uint8_t value, std::size_t lookCount) noexcept {
    if (value == 0 || lookCount == 0) {
        return std::nullopt;
    }
    // 1..255 aufgerundet auf lookCount gleich große Stufen verteilt (letzte Stufe kann etwas kleiner ausfallen).
    const std::size_t binSize = (255 + lookCount - 1) / lookCount;
    const std::size_t index = std::min<std::size_t>((static_cast<std::size_t>(value) - 1) / binSize, lookCount - 1);
    return static_cast<int>(index);
}

}  // namespace whitehole::runtime
