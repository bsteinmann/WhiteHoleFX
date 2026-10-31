// ddp.cpp: Umsetzung zu ddp.hpp. Baut DDP-Pakete (Distributed Display Protocol) für RGB-Pixeldaten.
#include "whitehole/output/ddp.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace whitehole::output {

namespace {

constexpr std::uint8_t kFlagVersion1 = 0x40;
constexpr std::uint8_t kFlagPush = 0x01;
constexpr std::uint8_t kTypeRgb8 = 0x0B;

}  // namespace

Result<std::vector<std::uint8_t>> encodeDdp(const DdpParams& params,
                                            std::span<const std::uint8_t> pixels) {
    if (pixels.empty() || pixels.size() > kDdpMaxPayload) {
        return Error{ErrorCode::InvalidArgument, "Ein DDP-Paket trägt 1 bis 1440 Bytes"};
    }
    if (params.sequence > 15) {
        return Error{ErrorCode::InvalidArgument, "Die DDP-Sequenz muss zwischen 0 und 15 liegen"};
    }

    std::vector<std::uint8_t> p(kDdpHeaderSize + pixels.size(), 0);
    p[0] = static_cast<std::uint8_t>(kFlagVersion1 | (params.push ? kFlagPush : 0U));
    p[1] = params.sequence;
    p[2] = kTypeRgb8;
    p[3] = params.destination;
    p[4] = static_cast<std::uint8_t>(params.offset >> 24U);
    p[5] = static_cast<std::uint8_t>((params.offset >> 16U) & 0xFFU);
    p[6] = static_cast<std::uint8_t>((params.offset >> 8U) & 0xFFU);
    p[7] = static_cast<std::uint8_t>(params.offset & 0xFFU);
    p[8] = static_cast<std::uint8_t>(pixels.size() >> 8U);
    p[9] = static_cast<std::uint8_t>(pixels.size() & 0xFFU);
    std::copy(pixels.begin(), pixels.end(), p.begin() + kDdpHeaderSize);
    return p;
}

Result<std::vector<std::vector<std::uint8_t>>> encodeDdpStream(
    const DdpParams& params, std::span<const std::uint8_t> pixels) {
    if (pixels.empty()) {
        return Error{ErrorCode::InvalidArgument, "Ein DDP-Strom braucht mindestens ein Byte"};
    }
    const std::uint64_t end = std::uint64_t{params.offset} + pixels.size();
    if (end > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::InvalidArgument, "Der DDP-Offset überschreitet 32 Bit"};
    }

    std::vector<std::vector<std::uint8_t>> packets;
    packets.reserve((pixels.size() + kDdpMaxPayload - 1) / kDdpMaxPayload);
    for (std::size_t done = 0; done < pixels.size(); done += kDdpMaxPayload) {
        const std::size_t count = std::min(kDdpMaxPayload, pixels.size() - done);
        DdpParams chunk = params;
        chunk.offset = params.offset + static_cast<std::uint32_t>(done);
        chunk.push = params.push && (done + count == pixels.size());
        Result<std::vector<std::uint8_t>> packet = encodeDdp(chunk, pixels.subspan(done, count));
        if (!packet.ok()) {
            return packet.error();
        }
        packets.push_back(std::move(packet.value()));
    }
    return packets;
}

}  // namespace whitehole::output
