#include "whitehole/output/sacn.hpp"

#include <algorithm>
#include <string>

namespace whitehole::output {

namespace {

void put16(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
    out[at] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[at + 1] = static_cast<std::uint8_t>(value & 0xFFU);
}

void put32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
    put16(out, at, value >> 16U);
    put16(out, at + 2, value & 0xFFFFU);
}

}  // namespace

Result<std::vector<std::uint8_t>> encodeSacn(const SacnParams& params,
                                             std::span<const std::uint8_t> channels) {
    if (channels.empty() || channels.size() > kDmxMaxChannels) {
        return Error{ErrorCode::InvalidArgument, "sACN überträgt 1 bis 512 Kanäle pro Universe"};
    }
    if (params.universe < 1 || params.universe > kSacnMaxUniverse) {
        return Error{ErrorCode::InvalidArgument, "Das sACN-Universe muss zwischen 1 und 63999 liegen"};
    }
    if (params.priority > 200) {
        return Error{ErrorCode::InvalidArgument, "Die sACN-Priorität darf höchstens 200 sein"};
    }

    const std::size_t size = kSacnHeaderSize + channels.size();
    std::vector<std::uint8_t> p(size, 0);

    // Root-Layer
    put16(p, 0, 0x0010);
    put16(p, 2, 0x0000);
    constexpr std::array<char, 9> acn{'A', 'S', 'C', '-', 'E', '1', '.', '1', '7'};
    std::copy(acn.begin(), acn.end(), p.begin() + 4);  // Rest bleibt 0 (12 Byte gesamt)
    put16(p, 16, 0x7000U | static_cast<std::uint32_t>(size - 16));
    put32(p, 18, 0x00000004);
    std::copy(params.cid.begin(), params.cid.end(), p.begin() + 22);

    // Framing-Layer
    put16(p, 38, 0x7000U | static_cast<std::uint32_t>(size - 38));
    put32(p, 40, 0x00000002);
    const std::size_t nameLength = std::min(params.sourceName.size(), kSacnSourceNameSize - 1);
    std::copy_n(params.sourceName.begin(), nameLength, p.begin() + 44);
    p[108] = params.priority;
    put16(p, 109, 0);  // keine Synchronisation
    p[111] = params.sequence;
    p[112] = 0;
    put16(p, 113, params.universe);

    // DMP-Layer
    put16(p, 115, 0x7000U | static_cast<std::uint32_t>(size - 115));
    p[117] = 0x02;
    p[118] = 0xA1;
    put16(p, 119, 0);
    put16(p, 121, 1);
    put16(p, 123, static_cast<std::uint32_t>(channels.size() + 1));
    p[125] = 0;  // Startcode
    std::copy(channels.begin(), channels.end(), p.begin() + kSacnHeaderSize);
    return p;
}

std::string sacnMulticastAddress(std::uint16_t universe) {
    return "239.255." + std::to_string(universe >> 8U) + "." + std::to_string(universe & 0xFFU);
}

}  // namespace whitehole::output
