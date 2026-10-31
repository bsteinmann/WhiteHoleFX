// led_info.cpp: siehe led_info.hpp.
#include "whitehole/runtime/led_info.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace whitehole::runtime {

std::optional<std::uint32_t> ledAtPixel(const mapping::LedMap& map, const preview::ViewFrame& frame, float pixelX,
                                        float pixelY, float maxDistance) {
    std::optional<std::uint32_t> best;
    float bestDistance = maxDistance * maxDistance;
    for (std::size_t i = 0; i < map.world.size(); ++i) {
        const preview::PixelPoint p = preview::toPixel(frame, map.world[i]);
        const float dx = p.x - pixelX;
        const float dy = p.y - pixelY;
        const float distance = dx * dx + dy * dy;
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = static_cast<std::uint32_t>(i);
        }
    }
    return best;
}

std::optional<LedInfo> describeLed(const project::Project& project, const mapping::LedMap& map, std::uint32_t index,
                                   std::span<const ColorRgba> colors) {
    if (index >= map.world.size()) {
        return std::nullopt;
    }
    LedInfo info;
    info.index = index;
    info.world = map.world[index];
    if (index < colors.size()) {
        info.color = colors[index];
    }
    for (const mapping::FixtureRange& range : map.ranges) {
        if (index >= range.first && index < range.first + range.count) {
            info.fixture = range.fixture;
            info.indexInFixture = index - range.first;
            info.fixtureLedCount = range.count;
            if (const mapping::Fixture* fixture = project.layout.find(range.fixture)) {
                info.fixtureName = fixture->name;
            }
            break;
        }
    }
    // Adresse aus dem Plan: Er weiß, wie ein Fixture auf Universen aufgeteilt wird.
    if (const auto plan = output::OutputPlan::create(map, project.patches); plan.ok()) {
        for (const output::OutputSlot& slot : plan.value().slots()) {
            for (const output::OutputRun& run : slot.runs) {
                if (index >= run.firstLed && index < run.firstLed + run.ledCount) {
                    LedAddress address;
                    address.protocol = slot.protocol;
                    address.destination = slot.destination;
                    const std::uint32_t offset = run.byteOffset + (index - run.firstLed) * 3U;
                    if (slot.protocol == output::Protocol::Ddp) {
                        address.byteOffset = slot.address + offset;
                    } else {
                        address.universe = slot.address;
                        address.firstChannel = offset + 1U;
                    }
                    info.address = address;
                }
            }
        }
    }
    return info;
}

std::string formatLedInfo(const LedInfo& info) {
    char line[160];
    std::string text;
    std::snprintf(line, sizeof line, "%s: LED %u von %u (gesamt Nr. %u)\n",
                  info.fixtureName.empty() ? "Fixture" : info.fixtureName.c_str(), info.indexInFixture + 1U,
                  info.fixtureLedCount, info.index + 1U);
    text += line;
    std::snprintf(line, sizeof line, "Position: %.2f / %.2f / %.2f\n", static_cast<double>(info.world.x),
                  static_cast<double>(info.world.y), static_cast<double>(info.world.z));
    text += line;
    if (info.address) {
        const LedAddress& a = *info.address;
        switch (a.protocol) {
            case output::Protocol::ArtNet:
            case output::Protocol::Sacn:
                std::snprintf(line, sizeof line, "%s: Ziel %u, Universe %u, Kanal %u-%u\n",
                              a.protocol == output::Protocol::ArtNet ? "Art-Net" : "sACN", a.destination, a.universe,
                              a.firstChannel, a.firstChannel + 2U);
                break;
            case output::Protocol::Ddp:
                std::snprintf(line, sizeof line, "DDP: Ziel %u, Byte %u-%u\n", a.destination, a.byteOffset,
                              a.byteOffset + 2U);
                break;
        }
        text += line;
    } else {
        text += "Nicht gepatcht (keine Ausgabe)\n";
    }
    if (info.color) {
        const auto byte = [](float v) { return static_cast<int>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); };
        std::snprintf(line, sizeof line, "Farbe: R %d  G %d  B %d", byte(info.color->r), byte(info.color->g),
                      byte(info.color->b));
        text += line;
    } else {
        text.pop_back();  // letzten Zeilenumbruch entfernen
    }
    return text;
}

}  // namespace whitehole::runtime
