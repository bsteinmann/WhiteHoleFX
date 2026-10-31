// latest_frame_mailbox.cpp: Umsetzung zu latest_frame_mailbox.hpp. Übergabe zwischen Engine-Thread und Output-Thread.
#include "whitehole/engine/latest_frame_mailbox.hpp"

#include <utility>

namespace whitehole::engine {

void LatestFrameMailbox::publish(PixelFrame frame) {
    {
        const std::lock_guard lock{mutex_};
        if (latest_.has_value()) {
            ++dropped_;  // der alte Frame wurde nie abgeholt
        }
        latest_ = std::move(frame);
    }
    ready_.notify_one();
}

std::optional<PixelFrame> LatestFrameMailbox::takeLatest() {
    const std::lock_guard lock{mutex_};
    return std::exchange(latest_, std::nullopt);
}

std::optional<PixelFrame> LatestFrameMailbox::waitForFrame(std::chrono::milliseconds timeout) {
    std::unique_lock lock{mutex_};
    ready_.wait_for(lock, timeout, [this] { return latest_.has_value(); });
    return std::exchange(latest_, std::nullopt);
}

std::uint64_t LatestFrameMailbox::droppedFrames() const {
    const std::lock_guard lock{mutex_};
    return dropped_;
}

}  // namespace whitehole::engine
