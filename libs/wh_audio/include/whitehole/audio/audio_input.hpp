// audio_input.hpp: die Audio-Quelle des Programms: ein Aufnahmegerät oder eine WAV-Datei in Schleife.
//
// Warum es diese Datei gibt: Jemand muss Samples holen, durch den Analysator schicken und das
// Ergebnis in den AudioStore legen. Das ist `AudioInput`, für Gerät und Datei gleichermaßen, damit
// Studio und Kommandozeile nur eine Sache kennen: start, stop, Statustext.
//
// Threading: Das Gerät ruft den Analysator auf dem Audio-Thread des Systems auf, die Datei läuft in
// einem eigenen Thread in Echtzeit (10-ms-Häppchen). `start*` und `stop` gehören dem aufrufenden
// (UI-)Thread; der Store ist von überall lesbar.
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "whitehole/audio/analyzer.hpp"
#include "whitehole/audio/audio_store.hpp"
#include "whitehole/audio/capture_device.hpp"
#include "whitehole/core/result.hpp"

namespace whitehole::audio {

class AudioInput {
public:
    explicit AudioInput(std::shared_ptr<AudioStore> store);
    ~AudioInput();
    AudioInput(const AudioInput&) = delete;
    AudioInput& operator=(const AudioInput&) = delete;

    /// Startet die Aufnahme von einem Gerät ("" = Standard, siehe openCapture). Eine laufende
    /// Quelle wird vorher gestoppt.
    Result<void> startDevice(std::string_view deviceId);
    /// Spielt eine WAV-Datei in Echtzeit und in Schleife.
    Result<void> startFile(const std::string& path);
    void stop();

    [[nodiscard]] bool running() const noexcept { return running_.load(); }
    [[nodiscard]] const std::shared_ptr<AudioStore>& store() const noexcept { return store_; }

private:
    void feed(std::span<const float> mono);

    std::shared_ptr<AudioStore> store_;
    std::atomic<bool> running_{false};
    std::unique_ptr<AudioAnalyzer> analyzer_;   // wird nur vom Audio-Thread benutzt
    std::unique_ptr<CaptureSession> session_;
    std::thread fileThread_;
    std::atomic<bool> stopFile_{false};
};

}  // namespace whitehole::audio
