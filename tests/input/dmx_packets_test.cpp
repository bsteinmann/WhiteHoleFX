// DMX-Eingang: Paketauswertung (Art-Net, sACN) und DmxStore. Die Pakete baut der Ausgabe-Encoder;
// so ist sichergestellt, dass Senden und Empfangen dasselbe Format meinen.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <vector>

#include "whitehole/input/dmx_packets.hpp"
#include "whitehole/input/dmx_store.hpp"
#include "whitehole/output/artnet.hpp"
#include "whitehole/output/sacn.hpp"

using namespace whitehole;
using namespace whitehole::input;

namespace {

std::vector<std::uint8_t> channels(std::size_t count) {
    std::vector<std::uint8_t> data(count);
    for (std::size_t i = 0; i < count; ++i) {
        data[i] = static_cast<std::uint8_t>((i * 7 + 1) & 0xFF);
    }
    return data;
}

}  // namespace

TEST_CASE("Art-Net: ein gesendetes ArtDmx wird mit Universe und Kanälen gelesen", "[input][artnet]") {
    const auto data = channels(512);
    const auto packet = output::encodeArtDmx(output::ArtDmxParams{0x1234, 5, 0}, data);
    REQUIRE(packet.ok());

    const auto frame = parseArtNet(packet.value());
    REQUIRE(frame.has_value());
    CHECK(frame->protocol == DmxProtocol::ArtNet);
    CHECK(frame->universe == 0x1234);
    CHECK(frame->channels == data);
}

TEST_CASE("Art-Net: kurze Pakete, fremde OpCodes und Müll werden verworfen", "[input][artnet]") {
    const auto good = output::encodeArtDmx(output::ArtDmxParams{3, 0, 0}, channels(10)).value();

    auto poll = good;
    poll[9] = 0x20;  // OpCode 0x2000 = ArtPoll
    poll[8] = 0x00;
    CHECK_FALSE(parseArtNet(poll).has_value());

    auto wrongId = good;
    wrongId[0] = 'X';
    CHECK_FALSE(parseArtNet(wrongId).has_value());

    auto truncated = good;
    truncated.resize(good.size() - 1);  // Länge im Kopf verspricht mehr, als da ist
    CHECK_FALSE(parseArtNet(truncated).has_value());

    CHECK_FALSE(parseArtNet(std::vector<std::uint8_t>{}).has_value());
    CHECK_FALSE(parseArtNet(std::vector<std::uint8_t>(4, 0xFF)).has_value());
}

TEST_CASE("sACN: ein gesendetes Datenpaket wird mit Universe, Priorität und Kanälen gelesen", "[input][sacn]") {
    const auto data = channels(300);
    output::SacnParams params;
    params.universe = 4242;
    params.priority = 150;
    const auto packet = output::encodeSacn(params, data);
    REQUIRE(packet.ok());

    const auto frame = parseSacn(packet.value());
    REQUIRE(frame.has_value());
    CHECK(frame->protocol == DmxProtocol::Sacn);
    CHECK(frame->universe == 4242);
    CHECK(frame->priority == 150);
    CHECK(frame->channels == data);
}

TEST_CASE("sACN: Vorschau, Stromende, anderer Startcode und Müll werden verworfen", "[input][sacn]") {
    const auto good = output::encodeSacn(output::SacnParams{}, channels(20)).value();
    REQUIRE(parseSacn(good).has_value());

    auto preview = good;
    preview[112] |= 0x80;
    CHECK_FALSE(parseSacn(preview).has_value());

    auto terminated = good;
    terminated[112] |= 0x40;
    CHECK_FALSE(parseSacn(terminated).has_value());

    auto otherStartCode = good;
    otherStartCode[125] = 0xDD;  // z. B. Per-Channel-Priority
    CHECK_FALSE(parseSacn(otherStartCode).has_value());

    auto truncated = good;
    truncated.resize(130);
    CHECK_FALSE(parseSacn(truncated).has_value());

    auto wrongId = good;
    wrongId[4] = 'X';
    CHECK_FALSE(parseSacn(wrongId).has_value());

    CHECK_FALSE(parseSacn(std::vector<std::uint8_t>(50, 0)).has_value());
}

TEST_CASE("DmxStore: Werte, Alter und getrennte Protokolle", "[input][store]") {
    auto now = std::chrono::steady_clock::time_point{} + std::chrono::seconds{100};
    DmxStore store{[&now] { return now; }};

    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 1, 1).has_value());  // noch nie ein Paket

    store.write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{10, 20, 30, 40, 50});
    now += std::chrono::milliseconds{500};

    const auto sample = store.read(DmxProtocol::ArtNet, 1, 2, 3);
    REQUIRE(sample.has_value());
    CHECK(sample->values[0] == 20);
    CHECK(sample->values[1] == 30);
    CHECK(sample->values[2] == 40);
    CHECK(sample->ageSeconds == 0.5);

    CHECK_FALSE(store.read(DmxProtocol::Sacn, 1, 1).has_value());  // gleiche Nummer, anderes Protokoll
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 2, 1).has_value());

    // Kanäle jenseits des Pakets sind 0, jenseits von 512 ungültig.
    CHECK(store.read(DmxProtocol::ArtNet, 1, 100)->values[0] == 0);
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 1, 0).has_value());
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 1, 512, 2).has_value());
    CHECK(store.read(DmxProtocol::ArtNet, 1, 512, 1).has_value());
    CHECK_FALSE(store.read(DmxProtocol::ArtNet, 1, 1, 5).has_value());

    store.write(DmxProtocol::Sacn, 7, std::vector<std::uint8_t>{1});
    CHECK(store.universes(DmxProtocol::ArtNet).size() == 1);
    CHECK(store.universes(DmxProtocol::Sacn).count(7) == 1);

    store.countIgnored();
    const DmxStats stats = store.stats();
    CHECK(stats.packets == 2);
    CHECK(stats.ignored == 1);
    CHECK(stats.lastProtocol == DmxProtocol::Sacn);
    CHECK(stats.lastUniverse == 7);
}
