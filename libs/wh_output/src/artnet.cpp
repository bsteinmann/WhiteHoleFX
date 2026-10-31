// artnet.cpp: Umsetzung zu artnet.hpp. Baut ein Art-Net-Paket (ArtDmx) aus Universe und Kanalwerten.
#include "whitehole/output/artnet.hpp"

#include <algorithm>
#include <array>

namespace whitehole::output {

Result<std::vector<std::uint8_t>> encodeArtDmx(const ArtDmxParams& params,
                                               std::span<const std::uint8_t> channels) {
    if (channels.empty() || channels.size() > kDmxMaxChannels) {
        return Error{ErrorCode::InvalidArgument, "Art-Net überträgt 1 bis 512 Kanäle pro Universe"};
    }
    if (params.portAddress > kArtNetMaxPortAddress) {
        return Error{ErrorCode::InvalidArgument, "Die Art-Net Port-Address darf höchstens 32767 sein"};
    }

    const std::size_t length = channels.size() + (channels.size() % 2);  // gerade Länge
    std::vector<std::uint8_t> packet(kArtNetHeaderSize + length, 0);

    constexpr std::array<std::uint8_t, 8> id{'A', 'r', 't', '-', 'N', 'e', 't', 0};
    std::copy(id.begin(), id.end(), packet.begin());
    packet[8] = 0x00;  // OpCode 0x5000, little-endian
    packet[9] = 0x50;
    packet[10] = 0x00;  // Protokollversion 14, big-endian
    packet[11] = 14;
    packet[12] = params.sequence;
    packet[13] = params.physical;
    packet[14] = static_cast<std::uint8_t>(params.portAddress & 0xFFU);  // SubUni
    packet[15] = static_cast<std::uint8_t>(params.portAddress >> 8U);    // Net
    packet[16] = static_cast<std::uint8_t>(length >> 8U);
    packet[17] = static_cast<std::uint8_t>(length & 0xFFU);
    std::copy(channels.begin(), channels.end(), packet.begin() + kArtNetHeaderSize);
    return packet;
}

}  // namespace whitehole::output
