// output_plan.cpp: Umsetzung zu output_plan.hpp. Verteilt die LEDs eines Layouts auf Universen und Pixelströme.
#include "whitehole/output/output_plan.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/sacn.hpp"

namespace whitehole::output {

namespace {

constexpr std::uint32_t kBytesPerLed = 3;

bool isDmx(Protocol protocol) noexcept { return protocol != Protocol::Ddp; }

Result<void> validate(const FixturePatch& patch) {
    if (patch.protocol == Protocol::ArtNet && patch.universe > kArtNetMaxPortAddress) {
        return Error{ErrorCode::InvalidArgument, "Art-Net: Universe über 32767"};
    }
    if (patch.protocol == Protocol::Sacn &&
        (patch.universe < 1 || patch.universe > kSacnMaxUniverse)) {
        return Error{ErrorCode::InvalidArgument, "sACN: Universe außerhalb von 1..63999"};
    }
    if (isDmx(patch.protocol) &&
        (patch.channel < 1 || patch.channel > kDmxMaxChannels - (kBytesPerLed - 1))) {
        return Error{ErrorCode::InvalidArgument,
                     "Der Startkanal muss zwischen 1 und 510 liegen, damit mindestens eine LED passt"};
    }
    if (patch.skipEvery > 0 && patch.skipCount == 0) {
        return Error{ErrorCode::InvalidArgument, "Überspringen: nach je N LEDs muss mindestens ein Pixel ausgelassen werden"};
    }
    return {};
}

std::uint32_t maxUniverse(Protocol protocol) noexcept {
    return protocol == Protocol::ArtNet ? kArtNetMaxPortAddress : kSacnMaxUniverse;
}

}  // namespace

Result<OutputPlan> OutputPlan::create(const mapping::LedMap& map,
                                      std::span<const FixturePatch> patches) {
    OutputPlan plan;
    plan.ledCount_ = map.size();

    std::set<std::uint64_t> patched;
    // Schlüssel: Protokoll, Ziel, Universe. DDP-Ströme brauchen keinen Schlüssel.
    std::map<std::tuple<Protocol, std::uint32_t, std::uint32_t>, std::size_t> slotIndex;
    // Belegte Byte-Bereiche je Slot (auch die übersprungener Pixel, die keinen Lauf bekommen).
    std::map<std::tuple<Protocol, std::uint32_t, std::uint32_t>, std::vector<std::pair<std::uint32_t, std::uint32_t>>> occupied;

    for (const FixturePatch& patch : patches) {
        if (Result<void> ok = validate(patch); !ok.ok()) {
            return ok.error();
        }
        if (!patched.insert(patch.fixture.value()).second) {
            return Error{ErrorCode::AlreadyExists, "Ein Fixture ist mehrfach gepatcht"};
        }
        const auto range = std::find_if(map.ranges.begin(), map.ranges.end(),
                                        [&](const mapping::FixtureRange& r) { return r.fixture == patch.fixture; });
        if (range == map.ranges.end()) {
            return Error{ErrorCode::NotFound, "Ein Patch verweist auf ein Fixture, das nicht im Layout liegt"};
        }
        Result<ColorEncoder> encoder = ColorEncoder::create(patch.color);
        if (!encoder.ok()) {
            return encoder.error();
        }
        const auto encoderIndex = static_cast<std::uint32_t>(plan.encoders_.size());
        plan.encoders_.push_back(std::move(encoder.value()));

        const std::uint32_t count = range->count;
        if (count == 0) {
            continue;
        }
        // LEDs in Gruppen, zwischen denen übersprungene Pixel liegen (FixturePatch::skipEvery); ohne Überspringen
        // ist das Fixture eine Gruppe. Jede Gruppe beginnt an ihrem Platz im Datenstrom (dataSlot).
        const std::uint32_t groupSize = patch.skipEvery == 0 ? count : patch.skipEvery;

        if (patch.protocol == Protocol::Ddp) {
            const std::uint64_t end = std::uint64_t{patch.ddpOffset} + std::uint64_t{dataSlots(patch, count)} * kBytesPerLed;
            if (end > std::numeric_limits<std::uint32_t>::max()) {
                return Error{ErrorCode::InvalidArgument, "Der DDP-Strom überschreitet 32 Bit"};
            }
            OutputSlot slot;
            slot.protocol = Protocol::Ddp;
            slot.destination = patch.destination;
            slot.address = patch.ddpOffset;
            slot.length = dataSlots(patch, count) * kBytesPerLed;
            for (std::uint32_t first = 0; first < count; first += groupSize) {
                const std::uint32_t size = std::min(groupSize, count - first);
                slot.runs.push_back({range->first + first, size, dataSlot(patch, first) * kBytesPerLed, encoderIndex});
            }
            plan.slots_.push_back(std::move(slot));
            continue;
        }

        // Belegte Kanäle prüfen: alle Plätze des Datenstroms, übersprungene Pixel eingeschlossen (sie hängen am selben
        // Kabel, ein anderes Fixture darf ihre Kanäle nicht bekommen). Danach entstehen die Läufe je LED-Gruppe.
        {
            std::uint32_t remaining = dataSlots(patch, count);
            std::uint32_t universe = patch.universe;
            std::uint32_t byteOffset = static_cast<std::uint32_t>(patch.channel - 1);
            while (remaining > 0) {
                if (universe > maxUniverse(patch.protocol)) {
                    return Error{ErrorCode::InvalidArgument, "Das Fixture läuft über das letzte Universe hinaus"};
                }
                const std::uint32_t fit = std::min<std::uint32_t>(
                    remaining, (static_cast<std::uint32_t>(kDmxMaxChannels) - byteOffset) / kBytesPerLed);
                const auto key = std::make_tuple(patch.protocol, patch.destination, universe);
                auto [it, inserted] = slotIndex.try_emplace(key, plan.slots_.size());
                if (inserted) {
                    OutputSlot slot;
                    slot.protocol = patch.protocol;
                    slot.destination = patch.destination;
                    slot.address = universe;
                    plan.slots_.push_back(std::move(slot));
                }
                const std::uint32_t bytes = fit * kBytesPerLed;
                std::vector<std::pair<std::uint32_t, std::uint32_t>>& taken = occupied[key];
                for (const auto& [from, to] : taken) {
                    if (byteOffset < to && from < byteOffset + bytes) {
                        return Error{ErrorCode::AlreadyExists, "Zwei Fixtures belegen dieselben Kanäle"};
                    }
                }
                taken.emplace_back(byteOffset, byteOffset + bytes);
                plan.slots_[it->second].length = std::max(plan.slots_[it->second].length, byteOffset + bytes);
                remaining -= fit;
                byteOffset = 0;
                ++universe;
            }
        }
        for (std::uint32_t first = 0; first < count; first += groupSize) {
            std::uint32_t remaining = std::min(groupSize, count - first);
            std::uint32_t nextLed = range->first + first;
            // Universe und Byte des ersten Platzes der Gruppe, nach derselben Rechnung wie ledAddress().
            const std::uint32_t slotInStream = dataSlot(patch, first);
            const auto total = static_cast<std::uint32_t>(kDmxMaxChannels);
            const std::uint32_t startByte = static_cast<std::uint32_t>(patch.channel - 1);
            const std::uint32_t firstFit = (total - startByte) / kBytesPerLed;
            std::uint32_t universe = patch.universe;
            std::uint32_t byteOffset = startByte;
            if (slotInStream < firstFit) {
                byteOffset += slotInStream * kBytesPerLed;
            } else {
                const std::uint32_t rest = slotInStream - firstFit;
                const std::uint32_t perUniverse = total / kBytesPerLed;
                universe += 1U + rest / perUniverse;
                byteOffset = (rest % perUniverse) * kBytesPerLed;
            }
            while (remaining > 0) {
                const std::uint32_t fit = std::min<std::uint32_t>(
                    remaining, (static_cast<std::uint32_t>(kDmxMaxChannels) - byteOffset) / kBytesPerLed);
                OutputSlot& slot = plan.slots_[slotIndex.at(std::make_tuple(patch.protocol, patch.destination, universe))];
                slot.runs.push_back({nextLed, fit, byteOffset, encoderIndex});
                remaining -= fit;
                nextLed += fit;
                byteOffset = 0;
                ++universe;
            }
        }
    }
    return plan;
}

Result<void> OutputPlan::render(std::span<const ColorRgba> colors,
                                std::vector<std::vector<std::uint8_t>>& out) const {
    if (colors.size() != ledCount_) {
        return Error{ErrorCode::InvalidArgument, "Die Zahl der Farben passt nicht zum Layout"};
    }
    out.resize(slots_.size());
    for (std::size_t s = 0; s < slots_.size(); ++s) {
        const OutputSlot& slot = slots_[s];
        std::vector<std::uint8_t>& data = out[s];
        data.assign(slot.length, 0);
        for (const OutputRun& run : slot.runs) {
            const ColorEncoder& encoder = encoders_[run.encoder];
            std::uint8_t* dst = data.data() + run.byteOffset;
            for (std::uint32_t i = 0; i < run.ledCount; ++i) {
                const std::array<std::uint8_t, 3> rgb = encoder.encode(colors[run.firstLed + i]);
                dst[0] = rgb[0];
                dst[1] = rgb[1];
                dst[2] = rgb[2];
                dst += kBytesPerLed;
            }
        }
    }
    return {};
}

}  // namespace whitehole::output
