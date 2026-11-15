// dmx_output_usage.cpp: Umsetzung zu dmx_output_usage.hpp.
#include "whitehole/runtime/dmx_output_usage.hpp"

#include <algorithm>

#include "whitehole/output/artnet.hpp"

namespace whitehole::runtime {

namespace {

constexpr std::uint32_t kBytesPerLed = 3;  // wie output_plan.cpp: eine LED belegt immer 3 Kanäle, nie geteilt

bool isDmxProtocol(output::Protocol protocol) noexcept { return protocol != output::Protocol::Ddp; }

/// Folgt demselben Aufteilen auf mehrere Universen wie OutputPlan::create (eine LED wird nie zwischen zwei
/// Universen getrennt; läuft das Fixture über, geht es ab Kanal 1 im nächsten Universe weiter) und prüft,
/// ob der angefragte Kanal dabei in diesem Patch landet.
bool fixturePatchCovers(const output::FixturePatch& patch, std::uint64_t ledCount, output::Protocol protocol,
                        std::uint32_t universe, std::uint32_t channel) {
    if (patch.protocol != protocol || !isDmxProtocol(patch.protocol) || ledCount == 0) {
        return false;
    }
    std::uint32_t remaining = output::dataSlots(patch, static_cast<std::uint32_t>(ledCount));
    std::uint32_t currentUniverse = patch.universe;
    std::uint32_t byteOffset = static_cast<std::uint32_t>(patch.channel - 1);
    const auto queryByte = static_cast<std::uint32_t>(channel - 1);
    while (remaining > 0) {
        if (byteOffset >= static_cast<std::uint32_t>(output::kDmxMaxChannels)) {
            break;  // ungültiger Patch (Startkanal zu hoch) - hier nicht werten, nur Belegung suchen
        }
        const std::uint32_t fit = std::min<std::uint32_t>(
            remaining, (static_cast<std::uint32_t>(output::kDmxMaxChannels) - byteOffset) / kBytesPerLed);
        if (fit == 0) {
            break;
        }
        const std::uint32_t bytes = fit * kBytesPerLed;
        if (currentUniverse == universe && queryByte >= byteOffset && queryByte < byteOffset + bytes) {
            return true;
        }
        remaining -= fit;
        byteOffset = 0;
        ++currentUniverse;
    }
    return false;
}

}  // namespace

std::optional<DmxOutputOccupant> findDmxOutputOccupant(const mapping::Layout& layout,
                                                        std::span<const output::FixturePatch> fixturePatches,
                                                        std::span<const output::DmxChannelPatch> dmxPatches,
                                                        output::Protocol protocol, std::uint32_t universe,
                                                        std::uint32_t channel) {
    for (const output::FixturePatch& patch : fixturePatches) {
        const mapping::Fixture* fixture = layout.find(patch.fixture);
        if (fixture == nullptr) {
            continue;  // z. B. mitten in einem Undo-Zwischenschritt - lieber nichts anzeigen als raten
        }
        if (fixturePatchCovers(patch, mapping::ledCount(fixture->shape), protocol, universe, channel)) {
            return DmxOutputOccupant{DmxOutputOccupant::Kind::Fixture, fixture->name, 0};
        }
    }
    for (const output::DmxChannelPatch& patch : dmxPatches) {
        if (patch.protocol != protocol || patch.universe != universe) {
            continue;
        }
        if (channel >= patch.channel && channel < static_cast<std::uint32_t>(patch.channel) + patch.count) {
            return DmxOutputOccupant{DmxOutputOccupant::Kind::Pool, std::string{}, patch.poolChannel};
        }
    }
    return std::nullopt;
}

}  // namespace whitehole::runtime
