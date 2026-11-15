// SentDmxStore: isolierte Tests ohne Netzwerk (write/readAll/universes, eigene Uhr für ageSeconds). Spiegelt
// tests/input/dmx_store_test.cpp.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <vector>

#include "whitehole/output/sent_dmx_store.hpp"

using namespace whitehole::output;

TEST_CASE("SentDmxStore: readAll liefert nullopt ohne Frame, danach die Werte") {
    SentDmxStore store;
    REQUIRE_FALSE(store.readAll(Protocol::ArtNet, 3).has_value());

    std::vector<std::uint8_t> packet(512, 0);
    packet[0] = 1;
    packet[255] = 128;
    packet[511] = 255;
    store.write(Protocol::ArtNet, 3, packet);

    const auto frame = store.readAll(Protocol::ArtNet, 3);
    REQUIRE(frame.has_value());
    CHECK(frame->channels[0] == 1);
    CHECK(frame->channels[255] == 128);
    CHECK(frame->channels[511] == 255);
    CHECK(frame->channels[1] == 0);
}

TEST_CASE("SentDmxStore: getrennt nach Protokoll, Alter aus der eigenen Uhr") {
    int secondsElapsed = 0;
    SentDmxStore store{[&secondsElapsed] {
        return std::chrono::steady_clock::time_point{} + std::chrono::seconds{secondsElapsed};
    }};

    const std::vector<std::uint8_t> artnetPacket{1, 2, 3};
    const std::vector<std::uint8_t> sacnPacket{9, 9, 9};
    store.write(Protocol::ArtNet, 1, artnetPacket);
    store.write(Protocol::Sacn, 1, sacnPacket);

    secondsElapsed = 5;
    const auto artnetFrame = store.readAll(Protocol::ArtNet, 1);
    const auto sacnFrame = store.readAll(Protocol::Sacn, 1);
    REQUIRE(artnetFrame.has_value());
    REQUIRE(sacnFrame.has_value());
    CHECK(artnetFrame->channels[0] == 1);
    CHECK(sacnFrame->channels[0] == 9);
    CHECK(artnetFrame->ageSeconds == 5.0);
    CHECK(sacnFrame->ageSeconds == 5.0);
    CHECK_FALSE(store.readAll(Protocol::ArtNet, 2).has_value());
}

TEST_CASE("SentDmxStore: universes nach Protokoll getrennt") {
    SentDmxStore store;
    CHECK(store.universes(Protocol::ArtNet).empty());

    const std::vector<std::uint8_t> packet{1};
    store.write(Protocol::ArtNet, 0, packet);
    store.write(Protocol::ArtNet, 5, packet);

    const auto universes = store.universes(Protocol::ArtNet);
    CHECK(universes.size() == 2);
    CHECK(universes.count(0) == 1);
    CHECK(universes.count(5) == 1);
    CHECK(store.universes(Protocol::Sacn).empty());
}
