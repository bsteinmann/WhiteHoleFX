#include "whitehole/runtime/summary.hpp"

#include <sstream>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/ddp.hpp"
#include "whitehole/output/sacn.hpp"
#include "whitehole/runtime/session.hpp"

namespace whitehole::runtime {

namespace {

class NullSink final : public output::IPacketSink {
public:
    Result<void> send(const output::Endpoint&, std::span<const std::uint8_t>) override { return {}; }
};

const char* protocolName(output::Protocol p) {
    switch (p) {
        case output::Protocol::ArtNet: return "Art-Net";
        case output::Protocol::Sacn: return "sACN";
        case output::Protocol::Ddp: return "DDP";
    }
    return "?";
}

std::uint16_t defaultPort(output::Protocol p) {
    switch (p) {
        case output::Protocol::ArtNet: return output::kArtNetPort;
        case output::Protocol::Sacn: return output::kSacnPort;
        case output::Protocol::Ddp: return output::kDdpPort;
    }
    return 0;
}

}  // namespace

Result<std::string> summarize(const project::Project& project) {
    NullSink sink;
    auto parts = buildOutput(project, sink);
    if (!parts.ok()) {
        return parts.error();
    }
    const output::OutputPlan& plan = parts.value().pipeline->plan();

    std::ostringstream out;
    out << "Fixtures: " << project.layout.fixtures().size() << ", LEDs: " << parts.value().map.world.size()
        << ", Bildrate: " << project.framesPerSecond << " fps\n";
    out << "Ausgabe: " << plan.slots().size() << " Pakete je Frame\n";
    for (const output::OutputSlot& slot : plan.slots()) {
        const output::Destination& d = project.destinations[slot.destination];
        const std::uint16_t port = d.port != 0 ? d.port : defaultPort(slot.protocol);
        out << "  " << protocolName(slot.protocol) << " -> ";
        if (d.address.empty()) {
            out << "Multicast";
        } else {
            out << d.address << ":" << port;
        }
        if (slot.protocol == output::Protocol::Ddp) {
            out << ", Offset " << slot.address;
        } else {
            out << ", Universe " << slot.address;
        }
        out << ", " << slot.length << " Bytes\n";
    }
    out << "Effekt: " << (project.target ? "gesetzt (output-Zeile)" : "FEHLT (keine output-Zeile)") << "\n";
    return out.str();
}

}  // namespace whitehole::runtime
