// capture_device.hpp: Aufnahme von einem Audio-Eingang des Betriebssystems.
//
// Warum es diese Datei gibt: Alles, was das Betriebssystem berührt (ALSA/PulseAudio/PipeWire unter
// Linux, WASAPI unter Windows), steckt hinter dieser kleinen Schnittstelle; Umsetzung über miniaudio
// in capture_device.cpp. Ohne Aufnahme-Unterstützung (CMake-Option WHITEHOLEFX_AUDIO_CAPTURE=OFF)
// liefert jede Funktion einen Fehler, und der Rest bleibt benutzbar (WAV-Datei als Quelle).
//
// Unter Linux erscheinen die "Monitor"-Quellen von PulseAudio/PipeWire (das, was der Rechner gerade
// abspielt) als gewöhnliche Eingänge; unter Windows gibt es dafür vorerst nur Geräte wie "Stereomix".
#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::audio {

struct AudioDeviceInfo {
    std::string id;    // Nummer in der Geräteliste ("0", "1", …); "" steht für das Standardgerät
    std::string name;
    bool isDefault{false};
};

/// Alle Aufnahmegeräte. Fehler (Unsupported), wenn dieses Programm ohne Aufnahme gebaut wurde.
[[nodiscard]] Result<std::vector<AudioDeviceInfo>> listCaptureDevices();

/// Wird auf dem Audio-Thread des Systems mit Mono-Samples (-1..1) aufgerufen. Nicht blockieren.
using SampleCallback = std::function<void(std::span<const float> mono)>;

/// Ein geöffnetes, noch nicht gestartetes Aufnahmegerät.
class CaptureSession {
public:
    virtual ~CaptureSession() = default;
    [[nodiscard]] virtual double sampleRate() const = 0;
    [[nodiscard]] virtual std::string deviceName() const = 0;
    /// Beginnt die Aufnahme. Beenden durch Zerstören der Sitzung.
    virtual Result<void> start(SampleCallback callback) = 0;
};

/// Öffnet ein Gerät: `id` ist eine Nummer aus `listCaptureDevices`, ein Teil des Namens (ohne
/// Rücksicht auf Groß-/Kleinschreibung) oder leer für das Standardgerät.
[[nodiscard]] Result<std::unique_ptr<CaptureSession>> openCapture(std::string_view id);

}  // namespace whitehole::audio
