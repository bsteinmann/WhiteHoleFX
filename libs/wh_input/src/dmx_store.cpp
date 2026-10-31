// dmx_store.cpp: Umsetzung zu dmx_store.hpp. Der zuletzt empfangene Zustand aller DMX-Universen.
#include "whitehole/input/dmx_store.hpp"

#include <algorithm>

namespace whitehole::input {

void DmxStore::write(DmxProtocol protocol, std::uint32_t universe, std::span<const std::uint8_t> channels) {
    const auto stamp = now_();
    const std::lock_guard<std::mutex> lock{mutex_};
    Universe& u = universes_[{static_cast<std::uint8_t>(protocol), universe}];
    const std::size_t n = std::min(channels.size(), kDmxChannels);
    std::copy_n(channels.begin(), n, u.data.begin());
    u.updated = stamp;
    ++stats_.packets;
    stats_.lastProtocol = protocol;
    stats_.lastUniverse = universe;
}

void DmxStore::countIgnored() {
    const std::lock_guard<std::mutex> lock{mutex_};
    ++stats_.ignored;
}

std::optional<DmxSample> DmxStore::read(DmxProtocol protocol, std::uint32_t universe, std::uint32_t firstChannel,
                                        std::uint32_t count) const {
    if (firstChannel < 1 || count < 1 || count > 4 || firstChannel + count - 1 > kDmxChannels) {
        return std::nullopt;
    }
    const auto stamp = now_();
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto it = universes_.find({static_cast<std::uint8_t>(protocol), universe});
    if (it == universes_.end()) {
        return std::nullopt;
    }
    DmxSample sample;
    for (std::uint32_t i = 0; i < count; ++i) {
        sample.values[i] = it->second.data[firstChannel - 1 + i];
    }
    sample.ageSeconds = std::chrono::duration<double>(stamp - it->second.updated).count();
    return sample;
}

std::map<std::uint32_t, double> DmxStore::universes(DmxProtocol protocol) const {
    const auto stamp = now_();
    std::map<std::uint32_t, double> result;
    const std::lock_guard<std::mutex> lock{mutex_};
    for (const auto& [key, universe] : universes_) {
        if (key.first == static_cast<std::uint8_t>(protocol)) {
            result[key.second] = std::chrono::duration<double>(stamp - universe.updated).count();
        }
    }
    return result;
}

DmxStats DmxStore::stats() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return stats_;
}

}  // namespace whitehole::input
