// DmxStore: isolierte Tests ohne Netzwerk (write/read/readAll/universes/stats, eigene Uhr für ageSeconds).
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <vector>

#include "whitehole/input/dmx_store.hpp"

using namespace whitehole::input;

TEST_CASE("DmxStore: read liefert nullopt ohne Paket, danach die Werte") {
    DmxStore store;
    REQUIRE_FALSE(store.read(DmxProtocol::ArtNet, 0, 1, 1).has_value());

    const std::vector<std::uint8_t> packet{10, 20, 30, 40};
    store.write(DmxProtocol::ArtNet, 0, packet);
    const auto sample = store.read(DmxProtocol::ArtNet, 0, 1, 4);
    REQUIRE(sample.has_value());
    CHECK(sample->values[0] == 10);
    CHECK(sample->values[1] == 20);
    CHECK(sample->values[2] == 30);
    CHECK(sample->values[3] == 40);
}

TEST_CASE("DmxStore: read lehnt mehr als 4 Kanäle und Bereiche außerhalb 1..512 ab") {
    DmxStore store;
    const std::vector<std::uint8_t> packet(512, 7);
    store.write(DmxProtocol::ArtNet, 0, packet);
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 0, 1, 5).has_value());
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 0, 0, 1).has_value());
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 0, 510, 4).has_value());  // 510..513 reicht über 512 hinaus
    CHECK(store.read(DmxProtocol::ArtNet, 0, 509, 4).has_value());       // 509..512 passt genau
}

TEST_CASE("DmxStore: readAll liefert alle 512 Kanäle in einem Zugriff") {
    DmxStore store;
    REQUIRE_FALSE(store.readAll(DmxProtocol::ArtNet, 3).has_value());

    std::vector<std::uint8_t> packet(512, 0);
    packet[0] = 1;
    packet[255] = 128;
    packet[511] = 255;
    store.write(DmxProtocol::ArtNet, 3, packet);

    const auto frame = store.readAll(DmxProtocol::ArtNet, 3);
    REQUIRE(frame.has_value());
    CHECK(frame->channels[0] == 1);
    CHECK(frame->channels[255] == 128);
    CHECK(frame->channels[511] == 255);
    CHECK(frame->channels[1] == 0);  // unbelegte Kanäle bleiben 0
}

TEST_CASE("DmxStore: readAll ist pro Protokoll getrennt und meldet das Alter") {
    int secondsElapsed = 0;
    DmxStore store{[&secondsElapsed] {
        return std::chrono::steady_clock::time_point{} + std::chrono::seconds{secondsElapsed};
    }};

    const std::vector<std::uint8_t> artnetPacket{1, 2, 3};
    const std::vector<std::uint8_t> sacnPacket{9, 9, 9};
    store.write(DmxProtocol::ArtNet, 1, artnetPacket);
    store.write(DmxProtocol::Sacn, 1, sacnPacket);

    secondsElapsed = 5;
    const auto artnetFrame = store.readAll(DmxProtocol::ArtNet, 1);
    const auto sacnFrame = store.readAll(DmxProtocol::Sacn, 1);
    REQUIRE(artnetFrame.has_value());
    REQUIRE(sacnFrame.has_value());
    CHECK(artnetFrame->channels[0] == 1);
    CHECK(sacnFrame->channels[0] == 9);
    CHECK(artnetFrame->ageSeconds == 5.0);
    CHECK(sacnFrame->ageSeconds == 5.0);
    CHECK_FALSE(store.readAll(DmxProtocol::ArtNet, 2).has_value());  // anderes Universe, nie geschrieben
}

TEST_CASE("DmxStore: universes und stats") {
    DmxStore store;
    CHECK(store.universes(DmxProtocol::ArtNet).empty());

    const std::vector<std::uint8_t> packet{1};
    store.write(DmxProtocol::ArtNet, 0, packet);
    store.write(DmxProtocol::ArtNet, 5, packet);
    store.countIgnored();

    const auto universes = store.universes(DmxProtocol::ArtNet);
    CHECK(universes.size() == 2);
    CHECK(universes.count(0) == 1);
    CHECK(universes.count(5) == 1);
    CHECK(store.universes(DmxProtocol::Sacn).empty());

    const DmxStats stats = store.stats();
    CHECK(stats.packets == 2);
    CHECK(stats.ignored == 1);
    REQUIRE(stats.lastProtocol.has_value());
    CHECK(*stats.lastProtocol == DmxProtocol::ArtNet);
    CHECK(stats.lastUniverse == 5);
}
