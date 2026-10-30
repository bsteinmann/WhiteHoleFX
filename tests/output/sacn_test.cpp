// sACN (E1.31): Byte-genauer Vergleich mit dem Paketformat.
#include <catch2/catch_test_macros.hpp>

#include <numeric>
#include <string>
#include <vector>

#include "whitehole/output/sacn.hpp"

using namespace whitehole;
using namespace whitehole::output;

using Bytes = std::vector<std::uint8_t>;

namespace {

std::uint16_t be16(const Bytes& b, std::size_t at) {
    return static_cast<std::uint16_t>((b[at] << 8U) | b[at + 1]);
}

}  // namespace

TEST_CASE("Ein kleines sACN-Paket hat alle Felder an der richtigen Stelle", "[output][sacn]") {
    SacnParams params;
    params.cid = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    params.sourceName = "Test";
    params.universe = 0x0102;
    params.priority = 150;
    params.sequence = 9;
    const Bytes channels{10, 20, 30};

    const Result<Bytes> r = encodeSacn(params, channels);
    REQUIRE(r.ok());
    const Bytes& p = r.value();
    REQUIRE(p.size() == 129);

    CHECK(be16(p, 0) == 0x0010);
    CHECK(be16(p, 2) == 0x0000);
    const std::string id(p.begin() + 4, p.begin() + 13);
    CHECK(id == "ASC-E1.17");
    CHECK(p[13] == 0);
    CHECK(p[14] == 0);
    CHECK(p[15] == 0);
    CHECK(be16(p, 16) == 0x7071);  // 129 - 16 = 113
    CHECK(be16(p, 18) == 0x0000);
    CHECK(be16(p, 20) == 0x0004);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK(p[22 + i] == i);
    }

    CHECK(be16(p, 38) == 0x705B);  // 129 - 38 = 91
    CHECK(be16(p, 40) == 0x0000);
    CHECK(be16(p, 42) == 0x0002);
    CHECK(std::string(p.begin() + 44, p.begin() + 48) == "Test");
    for (std::size_t i = 48; i < 108; ++i) {
        CHECK(p[i] == 0);
    }
    CHECK(p[108] == 150);
    CHECK(be16(p, 109) == 0);
    CHECK(p[111] == 9);
    CHECK(p[112] == 0);
    CHECK(be16(p, 113) == 0x0102);

    CHECK(be16(p, 115) == 0x700E);  // 129 - 115 = 14
    CHECK(p[117] == 0x02);
    CHECK(p[118] == 0xA1);
    CHECK(be16(p, 119) == 0);
    CHECK(be16(p, 121) == 1);
    CHECK(be16(p, 123) == 4);  // 3 Kanäle + Startcode
    CHECK(p[125] == 0);
    CHECK(Bytes(p.begin() + 126, p.end()) == channels);
}

TEST_CASE("Ein volles Universe hat die bekannten Längenfelder", "[output][sacn]") {
    const Bytes channels(512, 0xFF);
    SacnParams params;
    const Result<Bytes> r = encodeSacn(params, channels);
    REQUIRE(r.ok());
    const Bytes& p = r.value();
    CHECK(p.size() == 638);
    CHECK(be16(p, 16) == 0x726E);
    CHECK(be16(p, 38) == 0x7258);
    CHECK(be16(p, 115) == 0x720B);
    CHECK(be16(p, 123) == 513);
}

TEST_CASE("Zu lange Quellnamen werden auf 63 Byte gekürzt", "[output][sacn]") {
    SacnParams params;
    params.sourceName = std::string(100, 'x');
    const Bytes channels(1, 0);
    const Result<Bytes> r = encodeSacn(params, channels);
    REQUIRE(r.ok());
    for (std::size_t i = 44; i < 44 + 63; ++i) {
        CHECK(r.value()[i] == 'x');
    }
    CHECK(r.value()[44 + 63] == 0);  // Abschluss bleibt erhalten
}

TEST_CASE("Die Multicast-Adresse folgt dem Universe", "[output][sacn]") {
    CHECK(sacnMulticastAddress(1) == "239.255.0.1");
    CHECK(sacnMulticastAddress(0x0102) == "239.255.1.2");
    CHECK(sacnMulticastAddress(63999) == "239.255.249.255");
}

TEST_CASE("Ungültige sACN-Eingaben ergeben einen Fehler", "[output][sacn]") {
    const Bytes ch(4, 0);
    SacnParams p;
    p.universe = 0;
    CHECK(encodeSacn(p, ch).error().code == ErrorCode::InvalidArgument);
    p.universe = 64000;
    CHECK(encodeSacn(p, ch).error().code == ErrorCode::InvalidArgument);
    p.universe = 63999;
    CHECK(encodeSacn(p, ch).ok());
    p.priority = 201;
    CHECK(encodeSacn(p, ch).error().code == ErrorCode::InvalidArgument);
    p.priority = 200;
    CHECK(encodeSacn(p, ch).ok());
    const Bytes none;
    const Bytes tooMany(513, 0);
    CHECK(encodeSacn({}, none).error().code == ErrorCode::InvalidArgument);
    CHECK(encodeSacn({}, tooMany).error().code == ErrorCode::InvalidArgument);
}
