// output_pipeline_channels.cpp: DMX-Kanäle der Output-Nodes (ChannelWrite) in die Universen eintragen.
//
// Warum eine eigene Datei: output_pipeline.cpp kümmert sich um Farben, Pakete und Sequenznummern; hier steht nur,
// wie die Kanalwerte aus dem Node-Graph dazukommen. Zwei Fälle:
//   - Das Universe gehört schon einem Fixture (Slot im Plan): Die Bytes werden in dessen Puffer geschrieben, außer sie
//     überschneiden sich mit den LEDs; dann wird der Eintrag übersprungen und als Fehler gezählt (lastSendError), damit
//     die Oberfläche es melden kann. Kein Fixture-Kanal wird je überschrieben.
//   - Sonst ein eigener ChannelSlot je (Protokoll, Ziel, Universe), der zwischen den Frames bestehen bleibt, damit die
//     Sequenznummern weiterlaufen. Er wird nur gesendet, wenn ihn in diesem Frame jemand beschrieben hat; Universen
//     verschwundener Nodes hören also auf zu senden. Zwei Nodes im selben freien Universe dürfen sich ergänzen; schreiben
//     beide denselben Kanal, gewinnt der spätere.
// DDP-Ziele sind keine DMX-Ziele; ein Eintrag dorthin wird gezählt und übersprungen.
#include <algorithm>
#include <tuple>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/output/sacn.hpp"

namespace whitehole::output {

void OutputPipeline::applyChannels(std::span<const ChannelWrite> channels) {
    for (ChannelSlot& extra : channelSlots_) {
        extra.used = false;
        std::fill(extra.data.begin(), extra.data.end(), std::uint8_t{0});
    }
    for (const ChannelWrite& write : channels) {
        if (write.destination >= config_.destinations.size()) {
            noteError(Error{ErrorCode::InvalidArgument, "DMX-Node: der Controller " + std::to_string(write.destination) + " ist nicht angelegt"});
            continue;
        }
        const Destination& dest = config_.destinations[write.destination];
        const Protocol protocol = dest.protocol;
        if (protocol == Protocol::Ddp) {
            noteError(Error{ErrorCode::InvalidArgument, "DMX-Node: DDP-Controller haben keine DMX-Universen"});
            continue;
        }
        const std::uint32_t last = protocol == Protocol::ArtNet ? kArtNetMaxPortAddress : kSacnMaxUniverse;
        const std::uint32_t first = protocol == Protocol::ArtNet ? 0U : 1U;
        if (write.universe < first || write.universe > last || write.firstChannel < 1 || write.firstChannel > kDmxMaxChannels ||
            write.data.empty()) {
            noteError(Error{ErrorCode::InvalidArgument, "DMX-Node: Universe oder Kanal außerhalb des Bereichs"});
            continue;
        }
        const std::size_t from = write.firstChannel - 1U;
        const std::size_t count = std::min(write.data.size(), kDmxMaxChannels - from);
        const std::size_t to = from + count;

        // 1. Universe eines Fixtures?
        bool placed = false;
        for (std::size_t i = 0; i < plan_.slots().size() && !placed; ++i) {
            const OutputSlot& slot = plan_.slots()[i];
            if (slot.protocol != protocol || slot.destination != write.destination || slot.address != write.universe) {
                continue;
            }
            placed = true;
            bool overlaps = false;
            for (const OutputRun& run : slot.runs) {
                const std::size_t runEnd = run.byteOffset + run.ledCount * 3U;
                overlaps = overlaps || (from < runEnd && run.byteOffset < to);
            }
            if (overlaps) {
                noteError(Error{ErrorCode::AlreadyExists, "DMX-Node: Universe " + std::to_string(write.universe) + ", Kanal " +
                                                              std::to_string(write.firstChannel) + " gehört einem Fixture"});
                break;
            }
            std::vector<std::uint8_t>& data = slotData_[i];
            if (data.size() < to) {
                data.resize(to, 0);
            }
            std::copy_n(write.data.begin(), count, data.begin() + static_cast<std::ptrdiff_t>(from));
        }
        if (placed) {
            continue;
        }
        // 2. Eigenes Universe
        auto found = std::find_if(channelSlots_.begin(), channelSlots_.end(), [&](const ChannelSlot& c) {
            return c.slot.protocol == protocol && c.slot.destination == write.destination && c.slot.address == write.universe;
        });
        if (found == channelSlots_.end()) {
            Result<Endpoint> endpoint = resolveEndpoint(dest, protocol, write.universe);
            if (!endpoint.ok()) {
                noteError(endpoint.error());
                continue;
            }
            ChannelSlot fresh;
            fresh.slot.protocol = protocol;
            fresh.slot.destination = write.destination;
            fresh.slot.address = write.universe;
            fresh.endpoint = std::move(endpoint.value());
            fresh.sequence = protocol == Protocol::Sacn ? 0 : 1;
            channelSlots_.push_back(std::move(fresh));
            found = std::prev(channelSlots_.end());
        }
        if (found->data.size() < to) {
            found->data.resize(to, 0);
        }
        std::copy_n(write.data.begin(), count, found->data.begin() + static_cast<std::ptrdiff_t>(from));
        found->slot.length = static_cast<std::uint32_t>(std::max<std::size_t>(found->slot.length, to));
        found->used = true;
    }
}

}  // namespace whitehole::output
