// patch_space.cpp: Umsetzung zu patch_space.hpp.
#include "whitehole/output/patch_space.hpp"

#include <algorithm>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/sacn.hpp"

namespace whitehole::output {

namespace {

constexpr std::uint32_t kBytesPerLed = 3;
constexpr auto kTotal = static_cast<std::uint32_t>(kDmxMaxChannels);

std::uint32_t lastUniverseOf(Protocol protocol) {
    return protocol == Protocol::ArtNet ? kArtNetMaxPortAddress : kSacnMaxUniverse;
}

bool collides(std::span<const ChannelBlock> mine, std::span<const ChannelBlock> others) {
    return std::any_of(mine.begin(), mine.end(), [&](const ChannelBlock& a) {
        return std::any_of(others.begin(), others.end(), [&](const ChannelBlock& b) {
            return a.protocol == b.protocol && a.destination == b.destination && a.universe == b.universe &&
                   a.firstByte < b.endByte && b.firstByte < a.endByte;
        });
    });
}

}  // namespace

std::vector<ChannelBlock> channelBlocks(const FixturePatch& patch, std::uint32_t ledCount) {
    std::vector<ChannelBlock> blocks;
    if (patch.protocol == Protocol::Ddp || ledCount == 0) {
        return blocks;
    }
    std::uint32_t universe = patch.universe;
    std::uint32_t byte = patch.channel > 0 ? patch.channel - 1U : 0U;
    std::uint32_t remaining = dataSlots(patch, ledCount);  // übersprungene Pixel belegen ihre Kanäle mit
    while (remaining > 0 && universe <= lastUniverseOf(patch.protocol)) {
        const std::uint32_t fit = std::min(remaining, byte < kTotal ? (kTotal - byte) / kBytesPerLed : 0U);
        if (fit > 0) {
            blocks.push_back({patch.protocol, patch.destination, universe, byte, byte + fit * kBytesPerLed});
        }
        remaining -= fit;
        byte = 0;
        ++universe;
    }
    return blocks;
}

std::optional<AddressSpan> addressSpan(const FixturePatch& patch, std::uint32_t ledCount) {
    if (ledCount == 0) {
        return std::nullopt;
    }
    AddressSpan span;
    span.protocol = patch.protocol;
    if (patch.protocol == Protocol::Ddp) {
        span.firstChannel = patch.ddpOffset;
        span.lastChannel = patch.ddpOffset + dataSlots(patch, ledCount) * kBytesPerLed - 1U;
        return span;
    }
    const std::vector<ChannelBlock> blocks = channelBlocks(patch, ledCount);
    if (blocks.empty()) {
        return std::nullopt;
    }
    span.firstUniverse = blocks.front().universe;
    span.firstChannel = blocks.front().firstByte + 1U;
    span.lastUniverse = blocks.back().universe;
    span.lastChannel = blocks.back().endByte;
    return span;
}

std::optional<std::uint16_t> fitChannel(const FixturePatch& wanted, std::uint32_t ledCount,
                                        std::span<const ChannelBlock> others) {
    if (wanted.protocol == Protocol::Ddp || ledCount == 0) {
        return wanted.channel;
    }
    if (!collides(channelBlocks(wanted, ledCount), others)) {
        return wanted.channel;
    }
    std::uint32_t end = 0;
    for (const ChannelBlock& block : others) {
        if (block.protocol == wanted.protocol && block.destination == wanted.destination &&
            block.universe == wanted.universe) {
            end = std::max(end, block.endByte);
        }
    }
    if (end + kBytesPerLed > kTotal) {
        return std::nullopt;
    }
    FixturePatch moved = wanted;
    moved.channel = static_cast<std::uint16_t>(end + 1U);
    if (collides(channelBlocks(moved, ledCount), others)) {
        return std::nullopt;
    }
    return moved.channel;
}

}  // namespace whitehole::output
