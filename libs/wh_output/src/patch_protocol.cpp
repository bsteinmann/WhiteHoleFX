// patch_protocol.cpp: Umsetzung zu patch_protocol.hpp.
#include "whitehole/output/patch_protocol.hpp"

#include <algorithm>
#include <string>

namespace whitehole::output {

Result<int> alignPatchProtocol(FixturePatch& patch, Protocol protocol) {
    if (patch.protocol == protocol) {
        return 0;
    }
    int shift = 0;
    std::uint32_t universe = patch.universe;
    if (patch.protocol == Protocol::ArtNet && protocol == Protocol::Sacn) {
        shift = 1;
    } else if (patch.protocol == Protocol::Sacn && protocol == Protocol::ArtNet) {
        shift = universe > 0 ? -1 : 0;
    }
    universe = static_cast<std::uint32_t>(static_cast<std::int64_t>(universe) + shift);
    if (protocol != Protocol::Ddp) {
        universe = std::max<std::uint32_t>(universe, firstUniverse(protocol));
        if (universe > lastUniverse(protocol)) {
            return Error{ErrorCode::InvalidArgument,
                         "Universe " + std::to_string(patch.universe) + " gibt es im neuen Protokoll nicht (höchstens " +
                             std::to_string(lastUniverse(protocol)) + ")"};
        }
    }
    patch.universe = static_cast<std::uint16_t>(universe);
    patch.protocol = protocol;
    return shift;
}

}  // namespace whitehole::output
