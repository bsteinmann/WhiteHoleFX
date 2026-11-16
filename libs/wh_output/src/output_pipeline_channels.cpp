// output_pipeline_channels.cpp: DMX-Kanäle der Output-Nodes (ChannelWrite) über die Patches in die Universen eintragen.
//
// Warum eine eigene Datei: output_pipeline.cpp kümmert sich um Farben, Pakete und Sequenznummern; hier steht nur,
// wie die Kanalwerte aus dem Node-Graph dazukommen. Ein Write kennt nur seinen Platz im internen Kanal-Pool
// (write.poolChannel, siehe channel_write.hpp); applyChannels sucht für jeden überlappenden Bereich den passenden
// config_.dmxPatches-Eintrag (ein Write kann über mehrere Patches gestreut sein, wenn der Nutzer den Pool
// aufgeteilt hat) und reicht den übersetzten Block an applyResolvedWrite weiter. Pool-Kanäle ohne Patch werden
// stillschweigend nicht gesendet (wie ein ungepatchtes Fixture).
//
// applyResolvedWrite kennt zwei Fälle, wie zuvor:
//   - Das Universe gehört schon einem Fixture (Slot im Plan): Die Bytes werden in dessen Puffer geschrieben, außer sie
//     überschneiden sich mit den LEDs; dann wird der Eintrag übersprungen und als Fehler gezählt (lastSendError), damit
//     die Oberfläche es melden kann. Kein Fixture-Kanal wird je überschrieben.
//   - Sonst ein eigener ChannelSlot je (Protokoll, Ziel, Universe), der zwischen den Frames bestehen bleibt, damit die
//     Sequenznummern weiterlaufen. Er wird nur gesendet, wenn ihn in diesem Frame jemand beschrieben hat; Universen
//     verschwundener Nodes hören also auf zu senden. Zwei Nodes im selben freien Universe dürfen sich ergänzen; schreiben
//     beide denselben Kanal, gewinnt der spätere.
// Alle Ziel-/Universe-/Kanal-Werte sind an dieser Stelle schon durch OutputPipeline::create geprüft (dmxpatch-
// Validierung), darum keine erneute Bereichsprüfung hier.
#include <algorithm>
#include <tuple>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/output/sacn.hpp"

namespace whitehole::output {

void OutputPipeline::applyChannels(std::span<const ChannelWrite> channels) {
    for (ChannelSlot& extra : channelSlots_) {
        extra.wasUsed = extra.used;  // für den einen Nullen-Frame beim Verstummen, siehe sendAll()
        extra.used = false;
        std::fill(extra.data.begin(), extra.data.end(), std::uint8_t{0});
    }
    for (const ChannelWrite& write : channels) {
        if (write.data.empty()) {
            continue;
        }
        const std::size_t poolFrom = write.poolChannel;
        const std::size_t poolTo = poolFrom + write.data.size();
        for (const DmxChannelPatch& p : config_.dmxPatches) {
            const std::size_t patchFrom = p.poolChannel;
            const std::size_t patchTo = patchFrom + p.count;
            const std::size_t from = std::max(poolFrom, patchFrom);
            const std::size_t to = std::min(poolTo, patchTo);
            if (from >= to) {
                continue;  // dieser Patch deckt diesen Write nicht ab
            }
            const std::size_t dataOffset = from - poolFrom;
            const std::size_t count = to - from;
            const auto firstChannel = static_cast<std::uint16_t>(p.channel + (from - patchFrom));
            applyResolvedWrite(p.destination, p.protocol, p.universe, firstChannel,
                               std::span(write.data).subspan(dataOffset, count));
        }
    }
}

void OutputPipeline::applyResolvedWrite(std::uint32_t destination, Protocol protocol, std::uint32_t universe,
                                        std::uint16_t firstChannel, std::span<const std::uint8_t> data) {
    const std::size_t from = firstChannel - 1U;
    const std::size_t to = from + data.size();

    // 1. Universe eines Fixtures?
    bool placed = false;
    for (std::size_t i = 0; i < plan_.slots().size() && !placed; ++i) {
        const OutputSlot& slot = plan_.slots()[i];
        if (slot.protocol != protocol || slot.destination != destination || slot.address != universe) {
            continue;
        }
        placed = true;
        bool overlaps = false;
        for (const OutputRun& run : slot.runs) {
            const std::size_t runEnd = run.byteOffset + run.ledCount * 3U;
            overlaps = overlaps || (from < runEnd && run.byteOffset < to);
        }
        if (overlaps) {
            noteError(Error{ErrorCode::AlreadyExists, "DMX-Node: Universe " + std::to_string(universe) + ", Kanal " +
                                                          std::to_string(firstChannel) + " gehört einem Fixture"});
            break;
        }
        std::vector<std::uint8_t>& slotBytes = slotData_[i];
        if (slotBytes.size() < to) {
            slotBytes.resize(to, 0);
        }
        std::copy_n(data.begin(), data.size(), slotBytes.begin() + static_cast<std::ptrdiff_t>(from));
    }
    if (placed) {
        return;
    }
    // 2. Eigenes Universe
    auto found = std::find_if(channelSlots_.begin(), channelSlots_.end(), [&](const ChannelSlot& c) {
        return c.slot.protocol == protocol && c.slot.destination == destination && c.slot.address == universe;
    });
    if (found == channelSlots_.end()) {
        Result<Endpoint> endpoint = resolveEndpoint(config_.destinations[destination], protocol, universe);
        if (!endpoint.ok()) {
            noteError(endpoint.error());
            return;
        }
        ChannelSlot fresh;
        fresh.slot.protocol = protocol;
        fresh.slot.destination = destination;
        fresh.slot.address = universe;
        fresh.endpoint = std::move(endpoint.value());
        fresh.sequence = protocol == Protocol::Sacn ? 0 : 1;
        channelSlots_.push_back(std::move(fresh));
        found = std::prev(channelSlots_.end());
    }
    if (found->data.size() < to) {
        found->data.resize(to, 0);
    }
    std::copy_n(data.begin(), data.size(), found->data.begin() + static_cast<std::ptrdiff_t>(from));
    found->slot.length = static_cast<std::uint32_t>(std::max<std::size_t>(found->slot.length, to));
    found->used = true;
}

}  // namespace whitehole::output
