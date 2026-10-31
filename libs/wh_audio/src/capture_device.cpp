// capture_device.cpp: Umsetzung zu capture_device.hpp über miniaudio.
//
// Die Bibliothek selbst wird in miniaudio_impl.cpp übersetzt (ohne unsere strengen Warnungen);
// hier steht nur der Teil, der sie benutzt.
#include "whitehole/audio/capture_device.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

#include "miniaudio.h"

namespace whitehole::audio {

namespace {

Error fail(const std::string& message) { return Error{ErrorCode::IoError, "Audio: " + message}; }

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

/// Kontext mit Geräteliste; bleibt am Leben, solange ein Gerät offen ist.
struct Context {
    ma_context context{};
    bool ready{false};
    ma_device_info* devices{nullptr};
    ma_uint32 count{0};

    ~Context() {
        if (ready) {
            ma_context_uninit(&context);
        }
    }
    Result<void> open() {
        if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
            return fail("kein Audio-System gefunden");
        }
        ready = true;
        ma_device_info* playback = nullptr;
        ma_uint32 playbackCount = 0;
        if (ma_context_get_devices(&context, &playback, &playbackCount, &devices, &count) != MA_SUCCESS) {
            return fail("Geräteliste nicht lesbar");
        }
        return {};
    }
};

class MiniaudioSession final : public CaptureSession {
public:
    ~MiniaudioSession() override {
        if (deviceReady_) {
            ma_device_uninit(&device_);
        }
    }

    Result<void> open(std::string_view id) {
        if (Result<void> r = context_.open(); !r.ok()) {
            return r;
        }
        ma_device_config config = ma_device_config_init(ma_device_type_capture);
        config.capture.format = ma_format_f32;
        config.capture.channels = 1;  // miniaudio mischt Mehrkanal selbst zu Mono
        config.sampleRate = 0;        // Rate des Geräts
        config.dataCallback = &MiniaudioSession::onData;
        config.pUserData = this;

        ma_device_id chosen{};
        if (!id.empty()) {
            const std::string wanted = lower(std::string{id});
            std::size_t index = context_.count;
            if (std::all_of(wanted.begin(), wanted.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
                index = static_cast<std::size_t>(std::stoul(wanted));
            } else {
                for (ma_uint32 i = 0; i < context_.count; ++i) {
                    if (lower(context_.devices[i].name).find(wanted) != std::string::npos) {
                        index = i;
                        break;
                    }
                }
            }
            if (index >= context_.count) {
                return fail("Gerät '" + std::string{id} + "' nicht gefunden");
            }
            chosen = context_.devices[index].id;
            config.capture.pDeviceID = &chosen;
        }
        if (ma_device_init(&context_.context, &config, &device_) != MA_SUCCESS) {
            return fail("Gerät lässt sich nicht öffnen");
        }
        deviceReady_ = true;
        return {};
    }

    double sampleRate() const override { return static_cast<double>(device_.sampleRate); }
    std::string deviceName() const override {
        char name[256] = {};
        ma_device_get_name(const_cast<ma_device*>(&device_), ma_device_type_capture, name, sizeof(name), nullptr);
        return name;
    }

    Result<void> start(SampleCallback callback) override {
        callback_ = std::move(callback);
        if (ma_device_start(&device_) != MA_SUCCESS) {
            return fail("Aufnahme lässt sich nicht starten");
        }
        return {};
    }

private:
    static void onData(ma_device* device, void* /*output*/, const void* input, ma_uint32 frames) {
        auto* self = static_cast<MiniaudioSession*>(device->pUserData);
        if (input != nullptr && self->callback_) {
            self->callback_(std::span<const float>{static_cast<const float*>(input), frames});
        }
    }

    Context context_;
    ma_device device_{};
    bool deviceReady_{false};
    SampleCallback callback_;
};

}  // namespace

Result<std::vector<AudioDeviceInfo>> listCaptureDevices() {
    Context context;
    if (Result<void> r = context.open(); !r.ok()) {
        return r.error();
    }
    std::vector<AudioDeviceInfo> list;
    for (ma_uint32 i = 0; i < context.count; ++i) {
        list.push_back({std::to_string(i), context.devices[i].name, context.devices[i].isDefault != 0});
    }
    return list;
}

Result<std::unique_ptr<CaptureSession>> openCapture(std::string_view id) {
    auto session = std::make_unique<MiniaudioSession>();
    if (Result<void> r = session->open(id); !r.ok()) {
        return r.error();
    }
    return std::unique_ptr<CaptureSession>{std::move(session)};
}

}  // namespace whitehole::audio
