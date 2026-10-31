// audio_store.cpp: Umsetzung zu audio_store.hpp.
#include "whitehole/audio/audio_store.hpp"

namespace whitehole::audio {

void AudioStore::write(const AudioFrame& frame) {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    frame_ = frame;
    updated_ = now;
    status_.frames = frame.frameCount;
}

std::optional<AudioSnapshot> AudioStore::read() const {
    const auto now = now_();
    const std::lock_guard lock{mutex_};
    if (!frame_.has_value()) {
        return std::nullopt;
    }
    return AudioSnapshot{*frame_, std::chrono::duration<double>(now - updated_).count()};
}

void AudioStore::setSource(bool running, std::string description) {
    const std::lock_guard lock{mutex_};
    status_.running = running;
    status_.source = std::move(description);
}

AudioStatus AudioStore::status() const {
    const std::lock_guard lock{mutex_};
    return status_;
}

void AudioStore::clear() {
    const std::lock_guard lock{mutex_};
    frame_.reset();
    status_.frames = 0;
}

}  // namespace whitehole::audio
