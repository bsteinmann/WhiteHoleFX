// Art-Net ArtDmx: Byte-genauer Vergleich mit dem Paketformat.
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "whitehole/output/artnet.hpp"

using namespace whitehole;
using namespace whitehole::output;

using Bytes = std::vector<std::uint8_t>;

TEST_CASE("ArtDmx hat genau das spezifizierte Layout", "[output][artnet]") {
    ArtDmxParams params;
    params.portAddress = 0x1234;  // Net 0x12, SubUni 0x34
    params.sequence = 7;
    params.physical = 2;
    const Bytes channels{1, 2, 3, 4};

    const Result<Bytes> packet = encodeArtDmx(params, channels);
    REQUIRE(packet.ok());
    const Bytes expected{'A', 'r', 't', '-', 'N', 'e', 't', 0,  // Kennung
                         0x00, 0x50,                              // OpCode 0x5000, little-endian
                         0x00, 0x0E,                              // Version 14
                         7, 2,                                    // Sequence, Physical
                         0x34, 0x12,                              // SubUni, Net
                         0x00, 0x04,                              // Länge
                         1, 2, 3, 4};
    CHECK(packet.value() == expected);
}

TEST_CASE("Ungerade Kanalzahlen werden mit einer 0 aufgefüllt", "[output][artnet]") {
    const Bytes channels{9, 8, 7};
    const Result<Bytes> packet = encodeArtDmx({}, channels);
    REQUIRE(packet.ok());
    REQUIRE(packet.value().size() == kArtNetHeaderSize + 4);
    CHECK(packet.value()[16] == 0x00);
    CHECK(packet.value()[17] == 0x04);
    CHECK(packet.value()[18] == 9);
    CHECK(packet.value()[20] == 7);
    CHECK(packet.value()[21] == 0);
}

TEST_CASE("Ein volles Universe ist 530 Byte lang", "[output][artnet]") {
    const Bytes channels(512, 0xAB);
    ArtDmxParams params;
    params.portAddress = kArtNetMaxPortAddress;
    const Result<Bytes> packet = encodeArtDmx(params, channels);
    REQUIRE(packet.ok());
    CHECK(packet.value().size() == 530);
    CHECK(packet.value()[14] == 0xFF);
    CHECK(packet.value()[15] == 0x7F);
    CHECK(packet.value()[16] == 0x02);
    CHECK(packet.value()[17] == 0x00);
    CHECK(packet.value()[529] == 0xAB);
}

TEST_CASE("Ungültige Eingaben ergeben einen Fehler", "[output][artnet]") {
    const Bytes none;
    const Bytes tooMany(513, 0);
    const Bytes ok(2, 0);
    CHECK(encodeArtDmx({}, none).error().code == ErrorCode::InvalidArgument);
    CHECK(encodeArtDmx({}, tooMany).error().code == ErrorCode::InvalidArgument);
    ArtDmxParams bad;
    bad.portAddress = 0x8000;
    CHECK(encodeArtDmx(bad, ok).error().code == ErrorCode::InvalidArgument);
}
