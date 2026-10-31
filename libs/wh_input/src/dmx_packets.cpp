#include "whitehole/input/dmx_packets.hpp"

#include <algorithm>
#include <cstring>

namespace whitehole::input {

namespace {

constexpr std::size_t kArtNetHeader = 18;
constexpr std::size_t kSacnHeader = 126;  // bis einschließlich Startcode
constexpr std::uint8_t kArtId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};
constexpr std::uint8_t kSacnId[12] = {'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0};

std::uint16_t be16(std::span<const std::uint8_t> p, std::size_t at) {
    return static_cast<std::uint16_t>((p[at] << 8) | p[at + 1]);
}

std::uint32_t be32(std::span<const std::uint8_t> p, std::size_t at) {
    return (static_cast<std::uint32_t>(p[at]) << 24) | (static_cast<std::uint32_t>(p[at + 1]) << 16) |
           (static_cast<std::uint32_t>(p[at + 2]) << 8) | p[at + 3];
}

}  // namespace

std::optional<DmxFrame> parseArtNet(std::span<const std::uint8_t> packet) {
    if (packet.size() < kArtNetHeader || std::memcmp(packet.data(), kArtId, sizeof kArtId) != 0) {
        return std::nullopt;
    }
    const std::uint16_t opCode = static_cast<std::uint16_t>(packet[8] | (packet[9] << 8));  // little-endian
    if (opCode != 0x5000 || be16(packet, 10) < 14) {
        return std::nullopt;
    }
    const std::size_t length = be16(packet, 16);
    if (length < 2 || length > kDmxChannels || kArtNetHeader + length > packet.size()) {
        return std::nullopt;
    }
    DmxFrame frame;
    frame.protocol = DmxProtocol::ArtNet;
    frame.universe = static_cast<std::uint32_t>(packet[14] | ((packet[15] & 0x7F) << 8));
    frame.channels.assign(packet.begin() + kArtNetHeader, packet.begin() + static_cast<std::ptrdiff_t>(kArtNetHeader + length));
    return frame;
}

std::optional<DmxFrame> parseSacn(std::span<const std::uint8_t> packet) {
    if (packet.size() < kSacnHeader || be16(packet, 0) != 0x0010 || be16(packet, 2) != 0 ||
        std::memcmp(packet.data() + 4, kSacnId, sizeof kSacnId) != 0) {
        return std::nullopt;
    }
    if (be32(packet, 18) != 4 || be32(packet, 40) != 2) {  // Root: Datenpaket, Framing: Daten
        return std::nullopt;
    }
    if (packet[117] != 0x02 || packet[118] != 0xA1 || be16(packet, 119) != 0 || be16(packet, 121) != 1) {
        return std::nullopt;
    }
    const std::uint8_t options = packet[112];
    if ((options & 0x80) != 0 || (options & 0x40) != 0) {  // Preview, Ende des Stroms
        return std::nullopt;
    }
    const std::size_t count = be16(packet, 123);  // Kanäle + Startcode
    if (count < 2 || count > kDmxChannels + 1 || kSacnHeader - 1 + count > packet.size() || packet[125] != 0) {
        return std::nullopt;
    }
    const std::uint32_t universe = be16(packet, 113);
    if (universe < 1 || universe > 63999) {
        return std::nullopt;
    }
    DmxFrame frame;
    frame.protocol = DmxProtocol::Sacn;
    frame.universe = universe;
    frame.priority = packet[108];
    frame.channels.assign(packet.begin() + static_cast<std::ptrdiff_t>(kSacnHeader),
                          packet.begin() + static_cast<std::ptrdiff_t>(kSacnHeader - 1 + count));
    return frame;
}

}  // namespace whitehole::input
