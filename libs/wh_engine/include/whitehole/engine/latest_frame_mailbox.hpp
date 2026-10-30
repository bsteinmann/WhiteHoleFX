// latest_frame_mailbox.hpp: Übergabe zwischen Engine-Thread und Output-Thread.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>

#include "whitehole/engine/frame_sink.hpp"

namespace whitehole::engine {

/// Einsitziger, thread-sicherer Briefkasten: Der Engine-Thread legt Frames ab,
/// der Output-Thread holt immer den neuesten. Ein noch nicht abgeholter Frame
/// wird beim nächsten publish() verworfen. Latenz geht vor Vollständigkeit:
/// Ein langsamer Ausgang staut nie Frames auf.
class LatestFrameMailbox final : public IFrameSink {
public:
    void publish(PixelFrame frame) override;

    /// Nimmt den neuesten Frame, falls vorhanden. Blockiert nie.
    [[nodiscard]] std::optional<PixelFrame> takeLatest();

    /// Wartet auf einen Frame, höchstens bis das Timeout abläuft.
    [[nodiscard]] std::optional<PixelFrame> waitForFrame(std::chrono::milliseconds timeout);

    /// Anzahl der Frames, die überschrieben wurden, bevor sie abgeholt wurden.
    [[nodiscard]] std::uint64_t droppedFrames() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<PixelFrame> latest_;
    std::uint64_t dropped_{0};
};

}  // namespace whitehole::engine
