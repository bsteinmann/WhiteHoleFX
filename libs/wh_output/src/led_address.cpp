// led_address.cpp: Umsetzung zu led_address.hpp.
#include "whitehole/output/led_address.hpp"

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/sacn.hpp"

namespace whitehole::output {

namespace {
constexpr std::uint32_t kBytesPerLed = 3;
}

std::optional<LedAddress> ledAddress(const FixturePatch& patch, std::uint32_t ledInFixture) {
    LedAddress address;
    address.protocol = patch.protocol;
    address.destination = patch.destination;
    if (patch.protocol == Protocol::Ddp) {
        address.firstChannel = patch.ddpOffset + ledInFixture * kBytesPerLed;
        address.lastChannel = address.firstChannel + kBytesPerLed - 1;
        return address;
    }
    const auto total = static_cast<std::uint32_t>(kDmxMaxChannels);
    const std::uint32_t startByte = patch.channel - 1U;
    const std::uint32_t firstFit = (total - startByte) / kBytesPerLed;
    std::uint32_t universe = patch.universe;
    std::uint32_t byte = startByte;
    if (ledInFixture < firstFit) {
        byte += ledInFixture * kBytesPerLed;
    } else {
        const std::uint32_t rest = ledInFixture - firstFit;
        const std::uint32_t perUniverse = total / kBytesPerLed;
        universe += 1U + rest / perUniverse;
        byte = (rest % perUniverse) * kBytesPerLed;
    }
    const std::uint32_t maxUniverse = patch.protocol == Protocol::ArtNet ? kArtNetMaxPortAddress : kSacnMaxUniverse;
    if (universe > maxUniverse) {
        return std::nullopt;
    }
    address.universe = universe;
    address.firstChannel = byte + 1U;
    address.lastChannel = byte + kBytesPerLed;
    return address;
}

}  // namespace whitehole::output
