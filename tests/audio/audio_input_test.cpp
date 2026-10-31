// audio_input_test.cpp: prüft die Quelle "WAV-Datei" und die Geräteliste (ohne Hardware).
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "signal_support.hpp"
#include "whitehole/audio/audio_input.hpp"

using namespace whitehole;
using namespace std::chrono_literals;

namespace {

std::string writeWav(const std::string& name, const std::vector<float>& samples) {
    const auto path = (std::filesystem::temp_directory_path() / name).string();
    const auto bytes = test::wavBytes(samples);
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}  // namespace

TEST_CASE("AudioInput: eine WAV-Datei läuft in Echtzeit und füllt den Store", "[audio][input]") {
    const auto path = writeWav("wh_test_input.wav", test::sine(1000.0, 0.5, 0.5));
    auto store = std::make_shared<audio::AudioStore>();
    audio::AudioInput input{store};

    REQUIRE(input.startFile(path).ok());
    CHECK(input.running());
    CHECK(store->status().running);
    CHECK(store->status().source == "Datei: wh_test_input.wav");

    // Nach etwa 0,4 s müssen es um die 30 Analyseschritte sein (93 pro Sekunde); großzügig prüfen.
    std::this_thread::sleep_for(400ms);
    const auto snapshot = store->read();
    REQUIRE(snapshot.has_value());
    CHECK(snapshot->frame.frameCount >= 5);
    CHECK(snapshot->frame.rms > 0.2F);

    input.stop();
    CHECK_FALSE(input.running());
    CHECK_FALSE(store->status().running);
    CHECK_FALSE(store->read().has_value());
    std::filesystem::remove(path);
}

TEST_CASE("AudioInput: fehlende oder kaputte Datei meldet einen Fehler und startet nichts", "[audio][input]") {
    auto store = std::make_shared<audio::AudioStore>();
    audio::AudioInput input{store};
    CHECK_FALSE(input.startFile("/gibt/es/nicht.wav").ok());
    CHECK_FALSE(input.running());

    const auto path = (std::filesystem::temp_directory_path() / "wh_test_kaputt.wav").string();
    {
        std::ofstream file{path, std::ios::binary};
        file << "das ist keine WAV-Datei";
    }
    CHECK_FALSE(input.startFile(path).ok());
    CHECK_FALSE(input.running());
    std::filesystem::remove(path);
}

TEST_CASE("AudioInput: erneutes Starten ersetzt die Quelle, Zerstören stoppt sauber", "[audio][input]") {
    const auto a = writeWav("wh_test_a.wav", test::sine(440.0, 0.3));
    const auto b = writeWav("wh_test_b.wav", test::sine(880.0, 0.3));
    auto store = std::make_shared<audio::AudioStore>();
    {
        audio::AudioInput input{store};
        REQUIRE(input.startFile(a).ok());
        REQUIRE(input.startFile(b).ok());
        CHECK(store->status().source == "Datei: wh_test_b.wav");
    }  // Destruktor
    CHECK_FALSE(store->status().running);
    std::filesystem::remove(a);
    std::filesystem::remove(b);
}

TEST_CASE("Geräteliste: liefert eine Liste oder einen Fehler, stürzt aber nie ab", "[audio][input]") {
    const auto devices = audio::listCaptureDevices();
    if (devices.ok()) {
        for (const auto& device : devices.value()) {
            CHECK_FALSE(device.name.empty());
        }
    } else {
        CHECK_FALSE(devices.error().message.empty());
    }
    // Ein nicht vorhandenes Gerät darf nie gelingen.
    CHECK_FALSE(audio::openCapture("gibt-es-bestimmt-nicht-9999").ok());
}
