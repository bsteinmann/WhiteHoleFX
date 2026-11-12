// output_pipeline.cpp: Umsetzung zu output_pipeline.hpp. Vom Farbpuffer eines Frames bis zum letzten gesendeten Paket.
#include "whitehole/output/output_pipeline.hpp"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/ddp.hpp"
#include "whitehole/output/sacn.hpp"

namespace whitehole::output {

static_assert(sizeof(ColorRgba) == 16, "Die Abtast-Stage liefert 16 Byte je LED");
static_assert(std::is_trivially_copyable_v<ColorRgba>);

bool isIpv4(const std::string& text) noexcept {
    int parts = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t digits = 0;
        int value = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            value = value * 10 + (text[i] - '0');
            ++digits;
            ++i;
            if (digits > 3 || value > 255) {
                return false;
            }
        }
        if (digits == 0) {
            return false;
        }
        ++parts;
        if (i == text.size()) {
            break;
        }
        if (text[i] != '.') {
            return false;
        }
        ++i;
        if (i == text.size()) {
            return false;  // Punkt am Ende
        }
    }
    return parts == 4;
}

namespace {

std::uint16_t defaultPort(Protocol protocol) noexcept {
    switch (protocol) {
        case Protocol::ArtNet: return kArtNetPort;
        case Protocol::Sacn: return kSacnPort;
        case Protocol::Ddp: return kDdpPort;
    }
    return 0;
}

}  // namespace

Result<Endpoint> OutputPipeline::resolveEndpoint(const Destination& dest, Protocol protocol, std::uint32_t universe) {
    Endpoint endpoint;
    endpoint.port = dest.port != 0 ? dest.port : defaultPort(protocol);
    if (dest.address.empty()) {
        if (protocol != Protocol::Sacn) {
            return Error{ErrorCode::InvalidArgument, "Art-Net und DDP brauchen eine Zieladresse"};
        }
        endpoint.address = sacnMulticastAddress(static_cast<std::uint16_t>(universe));
        return endpoint;
    }
    if (!isIpv4(dest.address)) {
        return Error{ErrorCode::InvalidArgument,
                     "Die Zieladresse muss eine IPv4-Zahl sein (Rechnernamen werden nicht aufgelöst)"};
    }
    endpoint.address = dest.address;
    return endpoint;
}

OutputPipeline::OutputPipeline(OutputPlan plan, OutputConfig config, IPacketSink& sink,
                               std::vector<Endpoint> endpoints)
    : plan_{std::move(plan)},
      config_{std::move(config)},
      sink_{sink},
      endpoints_{std::move(endpoints)},
      colors_(plan_.ledCount()) {
    // Erste Sequenznummer je Protokoll: Art-Net und DDP beginnen bei 1 (0 hieße "aus"),
    // sACN bei 0.
    sequences_.reserve(plan_.slots().size());
    for (const OutputSlot& slot : plan_.slots()) {
        sequences_.push_back(slot.protocol == Protocol::Sacn ? 0 : 1);
    }
}

Result<std::unique_ptr<OutputPipeline>> OutputPipeline::create(OutputPlan plan, OutputConfig config,
                                                               IPacketSink& sink) {
    if (config.sacnPriority > 200) {
        return Error{ErrorCode::InvalidArgument, "Die sACN-Priorität darf höchstens 200 sein"};
    }
    std::vector<Endpoint> endpoints;
    endpoints.reserve(plan.slots().size());
    for (const OutputSlot& slot : plan.slots()) {
        if (slot.destination >= config.destinations.size()) {
            return Error{ErrorCode::InvalidArgument, "Ein Patch verweist auf ein Ziel, das nicht angelegt ist"};
        }
        Result<Endpoint> endpoint = resolveEndpoint(config.destinations[slot.destination], slot.protocol, slot.address);
        if (!endpoint.ok()) {
            return endpoint.error();
        }
        endpoints.push_back(std::move(endpoint.value()));
    }
    return std::unique_ptr<OutputPipeline>(
        new OutputPipeline(std::move(plan), std::move(config), sink, std::move(endpoints)));
}

Result<void> OutputPipeline::processBytes(std::span<const std::byte> bytes) {
    const std::size_t needed = plan_.ledCount() * kInputBytesPerLed;
    if (bytes.size() < needed) {
        return Error{ErrorCode::InvalidArgument, "Der Frame enthält weniger Bytes, als das Layout LEDs hat"};
    }
    if (needed > 0) {
        std::memcpy(colors_.data(), bytes.data(), needed);
    }
    return process(colors_);
}

Result<void> OutputPipeline::process(std::span<const ColorRgba> colors) {
    if (Result<void> ok = plan_.render(colors, slotData_); !ok.ok()) {
        return ok;
    }
    applyChannels({});
    sendAll();
    return {};
}

Result<void> OutputPipeline::processBytes(std::span<const std::byte> bytes, std::span<const ChannelWrite> channels) {
    const std::size_t needed = plan_.ledCount() * kInputBytesPerLed;
    if (bytes.size() < needed) {
        return Error{ErrorCode::InvalidArgument, "Der Frame enthält weniger Bytes, als das Layout LEDs hat"};
    }
    if (needed > 0) {
        std::memcpy(colors_.data(), bytes.data(), needed);
    }
    if (Result<void> ok = plan_.render(colors_, slotData_); !ok.ok()) {
        return ok;
    }
    applyChannels(channels);
    sendAll();
    return {};
}

void OutputPipeline::sendAll() {
    for (std::size_t i = 0; i < plan_.slots().size(); ++i) {
        sendSlot(plan_.slots()[i], slotData_[i], endpoints_[i], nextSequence(i));
    }
    for (ChannelSlot& extra : channelSlots_) {
        if (extra.used) {
            sendSlot(extra.slot, extra.data, extra.endpoint, advanceSequence(extra.slot.protocol, extra.sequence));
        }
    }
    frames_.fetch_add(1, std::memory_order_relaxed);
}

std::uint8_t OutputPipeline::advanceSequence(Protocol protocol, std::uint8_t& s) {
    const std::uint8_t current = s;
    switch (protocol) {
        case Protocol::ArtNet: s = (s == 255) ? 1 : static_cast<std::uint8_t>(s + 1); break;
        case Protocol::Ddp: s = (s >= 15) ? 1 : static_cast<std::uint8_t>(s + 1); break;
        case Protocol::Sacn: s = static_cast<std::uint8_t>(s + 1); break;  // 255 -> 0
    }
    return current;
}

std::uint8_t OutputPipeline::nextSequence(std::size_t slot) {
    return advanceSequence(plan_.slots()[slot].protocol, sequences_[slot]);
}

void OutputPipeline::sendSlot(const OutputSlot& slot, std::span<const std::uint8_t> data, const Endpoint& to,
                              std::uint8_t sequence) {
    switch (slot.protocol) {
        case Protocol::ArtNet: {
            ArtDmxParams p;
            p.portAddress = static_cast<std::uint16_t>(slot.address);
            p.sequence = sequence;
            Result<std::vector<std::uint8_t>> packet = encodeArtDmx(p, data);
            if (packet.ok()) {
                deliver(to, packet.value());
            } else {
                noteError(packet.error());
            }
            break;
        }
        case Protocol::Sacn: {
            SacnParams p;
            p.cid = config_.cid;
            p.sourceName = config_.sourceName;
            p.universe = static_cast<std::uint16_t>(slot.address);
            p.priority = config_.sacnPriority;
            p.sequence = sequence;
            Result<std::vector<std::uint8_t>> packet = encodeSacn(p, data);
            if (packet.ok()) {
                deliver(to, packet.value());
            } else {
                noteError(packet.error());
            }
            break;
        }
        case Protocol::Ddp: {
            DdpParams p;
            p.offset = slot.address;
            p.sequence = sequence;
            p.push = true;
            Result<std::vector<std::vector<std::uint8_t>>> packets = encodeDdpStream(p, data);
            if (!packets.ok()) {
                noteError(packets.error());
                break;
            }
            for (const std::vector<std::uint8_t>& packet : packets.value()) {
                deliver(to, packet);
            }
            break;
        }
    }
}

void OutputPipeline::deliver(const Endpoint& to, std::span<const std::uint8_t> packet) {
    const Result<void> sent = sink_.send(to, packet);
    if (sent.ok()) {
        packets_.fetch_add(1, std::memory_order_relaxed);
    } else {
        noteError(sent.error());
    }
}

void OutputPipeline::noteError(const Error& error) {
    errors_.fetch_add(1, std::memory_order_relaxed);
    const std::lock_guard<std::mutex> lock{errorMutex_};
    lastError_ = error;
}

OutputStats OutputPipeline::stats() const noexcept {
    return {frames_.load(std::memory_order_relaxed), packets_.load(std::memory_order_relaxed),
            errors_.load(std::memory_order_relaxed)};
}

std::optional<Error> OutputPipeline::lastSendError() const {
    const std::lock_guard<std::mutex> lock{errorMutex_};
    return lastError_;
}

}  // namespace whitehole::output
