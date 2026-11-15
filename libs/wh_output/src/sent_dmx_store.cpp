// sent_dmx_store.cpp: Umsetzung zu sent_dmx_store.hpp.
#include "whitehole/output/sent_dmx_store.hpp"

#include <algorithm>

namespace whitehole::output {

void SentDmxStore::write(Protocol protocol, std::uint32_t universe, std::span<const std::uint8_t> channels) {
    const auto stamp = now_();
    const std::lock_guard<std::mutex> lock{mutex_};
    Universe& u = universes_[{static_cast<std::uint8_t>(protocol), universe}];
    const std::size_t n = std::min(channels.size(), kSentDmxChannels);
    std::copy_n(channels.begin(), n, u.data.begin());
    u.updated = stamp;
}

std::optional<SentDmxFrame> SentDmxStore::readAll(Protocol protocol, std::uint32_t universe) const {
    const auto stamp = now_();
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto it = universes_.find({static_cast<std::uint8_t>(protocol), universe});
    if (it == universes_.end()) {
        return std::nullopt;
    }
    SentDmxFrame frame;
    frame.channels = it->second.data;
    frame.ageSeconds = std::chrono::duration<double>(stamp - it->second.updated).count();
    return frame;
}

std::map<std::uint32_t, double> SentDmxStore::universes(Protocol protocol) const {
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

}  // namespace whitehole::output
