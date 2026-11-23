// dmx_learn.cpp: Umsetzung zu dmx_learn.hpp.
#include "whitehole/input/dmx_learn.hpp"

#include <cstdlib>

namespace whitehole::input {

namespace {
constexpr std::array<DmxProtocol, 2> kProtocols{DmxProtocol::ArtNet, DmxProtocol::Sacn};
}  // namespace

void DmxLearn::arm() {
    baseline_.clear();
    for (const DmxProtocol protocol : kProtocols) {
        for (const auto& [universe, age] : store_->universes(protocol)) {
            (void)age;
            if (const auto frame = store_->readAll(protocol, universe)) {
                baseline_[Key{static_cast<std::uint8_t>(protocol), universe}] = frame->channels;
            }
        }
    }
    armed_ = true;
}

std::optional<DmxAddress> DmxLearn::poll() {
    if (!armed_) {
        return std::nullopt;
    }
    std::optional<DmxAddress> best;
    int bestChange = kMinChange - 1;
    for (const DmxProtocol protocol : kProtocols) {
        for (const auto& [universe, age] : store_->universes(protocol)) {
            (void)age;
            const auto frame = store_->readAll(protocol, universe);
            if (!frame) {
                continue;
            }
            const auto base = baseline_.find(Key{static_cast<std::uint8_t>(protocol), universe});
            for (std::size_t i = 0; i < kDmxChannels; ++i) {
                const int before = base == baseline_.end() ? 0 : base->second[i];
                const int change = std::abs(int{frame->channels[i]} - before);
                if (change > bestChange) {
                    bestChange = change;
                    best = DmxAddress{protocol, universe, static_cast<std::uint32_t>(i + 1)};
                }
            }
        }
    }
    if (best) {
        armed_ = false;
    }
    return best;
}

}  // namespace whitehole::input
