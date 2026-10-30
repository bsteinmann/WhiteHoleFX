// recording_sink.hpp: ein IPacketSink für Tests. Sammelt alle Pakete, statt zu senden.
#pragma once

#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "whitehole/output/packet_sink.hpp"

namespace whitehole::testing {

struct SentPacket {
    output::Endpoint to;
    std::vector<std::uint8_t> bytes;
};

class RecordingSink final : public output::IPacketSink {
public:
    Result<void> send(const output::Endpoint& to, std::span<const std::uint8_t> packet) override {
        const std::lock_guard<std::mutex> lock{mutex_};
        if (failing_.count(to.address) != 0) {
            return Error{ErrorCode::IoError, "Testfehler beim Senden"};
        }
        sent_.push_back({to, std::vector<std::uint8_t>(packet.begin(), packet.end())});
        return {};
    }

    /// Alle Pakete an diese Adresse schlagen ab jetzt fehl.
    void failFor(const std::string& address) {
        const std::lock_guard<std::mutex> lock{mutex_};
        failing_.insert(address);
    }

    [[nodiscard]] std::vector<SentPacket> packets() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return sent_;
    }

    [[nodiscard]] std::size_t count() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return sent_.size();
    }

    void clear() {
        const std::lock_guard<std::mutex> lock{mutex_};
        sent_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<SentPacket> sent_;
    std::set<std::string> failing_;
};

}  // namespace whitehole::testing
