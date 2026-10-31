// wav_test.cpp: prüft den WAV-Leser mit selbst gebauten Dateien.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "signal_support.hpp"
#include "whitehole/audio/wav.hpp"

using namespace whitehole;
using Catch::Approx;

TEST_CASE("WAV: 16-Bit-Mono wird gelesen", "[audio][wav]") {
    const auto samples = test::sine(440.0, 0.1, 0.5, 44100.0);
    const auto bytes = test::wavBytes(samples, 44100);
    const auto wav = audio::parseWav(bytes);
    REQUIRE(wav.ok());
    CHECK(wav.value().sampleRate == 44100.0);
    REQUIRE(wav.value().mono.size() == samples.size());
    CHECK(wav.value().mono[100] == Approx(samples[100]).margin(0.001));
}

TEST_CASE("WAV: Stereo wird zu Mono gemittelt", "[audio][wav]") {
    const auto samples = test::sine(440.0, 0.05, 0.5);
    const auto wav = audio::parseWav(test::wavBytes(samples, 48000, true));
    REQUIRE(wav.ok());
    REQUIRE(wav.value().mono.size() == samples.size());
    CHECK(wav.value().mono[200] == Approx(samples[200]).margin(0.001));
}

TEST_CASE("WAV: kaputte und fremde Dateien werden abgelehnt", "[audio][wav]") {
    CHECK_FALSE(audio::parseWav(std::vector<std::uint8_t>{}).ok());
    CHECK_FALSE(audio::parseWav(std::vector<std::uint8_t>(64, 0)).ok());

    auto bytes = test::wavBytes(test::sine(440.0, 0.01));
    auto noData = bytes;
    noData[36] = 'x';  // aus "data" wird "xata"
    CHECK_FALSE(audio::parseWav(noData).ok());

    auto compressed = bytes;
    compressed[20] = 2;  // Format 2 = ADPCM
    const auto result = audio::parseWav(compressed);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("WAV: abgeschnittene Datei liefert, was da ist", "[audio][wav]") {
    auto bytes = test::wavBytes(test::sine(440.0, 0.1));
    bytes.resize(bytes.size() - 1000);
    const auto wav = audio::parseWav(bytes);
    REQUIRE(wav.ok());
    CHECK(wav.value().mono.size() == 4800 - 500);
}

TEST_CASE("WAV: Datei lesen, fehlende Datei meldet IoError", "[audio][wav]") {
    const auto path = (std::filesystem::temp_directory_path() / "wh_test_audio.wav").string();
    {
        const auto bytes = test::wavBytes(test::sine(440.0, 0.1));
        std::ofstream file{path, std::ios::binary};
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    const auto wav = audio::readWavFile(path);
    CHECK(wav.ok());
    std::filesystem::remove(path);

    const auto missing = audio::readWavFile(path);
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().code == ErrorCode::IoError);
}
