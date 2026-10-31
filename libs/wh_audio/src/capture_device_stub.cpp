// capture_device_stub.cpp: Ersatz für capture_device.cpp, wenn ohne Audio-Aufnahme gebaut wird
// (CMake-Option WHITEHOLEFX_AUDIO_CAPTURE=OFF). Alles meldet "Unsupported"; WAV-Quellen laufen weiter.
#include "whitehole/audio/capture_device.hpp"

namespace whitehole::audio {

namespace {
Error unsupported() {
    return Error{ErrorCode::Unsupported, "Audio-Aufnahme: dieses Programm wurde ohne Aufnahme-Unterstützung gebaut"};
}
}  // namespace

Result<std::vector<AudioDeviceInfo>> listCaptureDevices() { return unsupported(); }
Result<std::unique_ptr<CaptureSession>> openCapture(std::string_view /*id*/) { return unsupported(); }

}  // namespace whitehole::audio
