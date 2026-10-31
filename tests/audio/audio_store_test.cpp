// audio_store_test.cpp: prüft den Speicher zwischen Audio-Thread und Nodes.
#include <catch2/catch_test_macros.hpp>
#include <chrono>

#include "whitehole/audio/audio_store.hpp"

using namespace whitehole;
using namespace std::chrono_literals;

TEST_CASE("AudioStore: leer, bis etwas geschrieben wird; Alter folgt der Uhr", "[audio][store]") {
    auto now = std::chrono::steady_clock::time_point{} + 100s;
    audio::AudioStore store{[&now] { return now; }};
    CHECK_FALSE(store.read().has_value());

    audio::AudioFrame frame;
    frame.rms = 0.25F;
    frame.frameCount = 7;
    store.write(frame);
    now += 500ms;
    const auto snapshot = store.read();
    REQUIRE(snapshot.has_value());
    CHECK(snapshot->frame.rms == 0.25F);
    CHECK(snapshot->ageSeconds > 0.49);
    CHECK(snapshot->ageSeconds < 0.51);
    CHECK(store.status().frames == 7);
}

TEST_CASE("AudioStore: Quelle und clear", "[audio][store]") {
    audio::AudioStore store;
    store.setSource(true, "Datei: beat.wav");
    CHECK(store.status().running);
    CHECK(store.status().source == "Datei: beat.wav");

    store.write(audio::AudioFrame{});
    store.clear();
    CHECK_FALSE(store.read().has_value());
    CHECK(store.status().frames == 0);
}
