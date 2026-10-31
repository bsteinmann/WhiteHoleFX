// audio_input.cpp: Umsetzung zu audio_input.hpp.
#include "whitehole/audio/audio_input.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "whitehole/audio/wav.hpp"

namespace whitehole::audio {

AudioInput::AudioInput(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

AudioInput::~AudioInput() { stop(); }

void AudioInput::feed(std::span<const float> mono) {
    if (analyzer_->push(mono) > 0) {
        store_->write(analyzer_->frame());
    }
}

Result<void> AudioInput::startDevice(std::string_view deviceId) {
    stop();
    Result<std::unique_ptr<CaptureSession>> opened = openCapture(deviceId);
    if (!opened.ok()) {
        return opened.error();
    }
    session_ = std::move(opened.value());
    AnalyzerConfig config;
    config.sampleRate = session_->sampleRate();
    analyzer_ = std::make_unique<AudioAnalyzer>(config);
    store_->clear();
    if (Result<void> started = session_->start([this](std::span<const float> mono) { feed(mono); }); !started.ok()) {
        session_.reset();
        analyzer_.reset();
        return started;
    }
    running_ = true;
    store_->setSource(true, "Gerät: " + session_->deviceName());
    return {};
}

Result<void> AudioInput::startFile(const std::string& path) {
    stop();
    Result<WavData> wav = readWavFile(path);
    if (!wav.ok()) {
        return wav.error();
    }
    if (wav.value().mono.empty()) {
        return Error{ErrorCode::InvalidArgument, "WAV-Datei enthält keine Samples: " + path};
    }
    AnalyzerConfig config;
    config.sampleRate = wav.value().sampleRate;
    analyzer_ = std::make_unique<AudioAnalyzer>(config);
    store_->clear();

    stopFile_ = false;
    running_ = true;
    const std::filesystem::path fsPath{std::u8string{reinterpret_cast<const char8_t*>(path.data()), path.size()}};
    const std::u8string name = fsPath.filename().u8string();  // UTF-8 auch unter Windows
    store_->setSource(true, "Datei: " + std::string{reinterpret_cast<const char*>(name.data()), name.size()});
    fileThread_ = std::thread{[this, data = std::move(wav.value())] {
        const auto chunk = std::max<std::size_t>(1, static_cast<std::size_t>(data.sampleRate / 100.0));  // 10 ms
        const auto begin = std::chrono::steady_clock::now();
        std::size_t position = 0;
        std::uint64_t played = 0;
        while (!stopFile_.load()) {
            const std::size_t n = std::min(chunk, data.mono.size() - position);
            feed(std::span<const float>{data.mono.data() + position, n});
            position = (position + n) % data.mono.size();
            played += n;
            // Auf die Echtzeituhr takten: das n-te Häppchen ist zur Zeit played/Rate fällig.
            std::this_thread::sleep_until(begin + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                      std::chrono::duration<double>(static_cast<double>(played) / data.sampleRate)));
        }
    }};
    return {};
}

void AudioInput::stop() {
    stopFile_ = true;
    if (fileThread_.joinable()) {
        fileThread_.join();
    }
    session_.reset();  // beendet die Aufnahme; danach ruft niemand mehr feed() auf
    analyzer_.reset();
    if (running_.exchange(false)) {
        store_->setSource(false, "");
        store_->clear();
    }
}

}  // namespace whitehole::audio
