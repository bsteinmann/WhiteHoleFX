// DDP: Header, Push-Flag, Offsets und Aufteilung in Pakete.
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "whitehole/output/ddp.hpp"

using namespace whitehole;
using namespace whitehole::output;

using Bytes = std::vector<std::uint8_t>;

TEST_CASE("Ein DDP-Paket hat den spezifizierten Header", "[output][ddp]") {
    DdpParams params;
    params.offset = 0x01020304;
    params.sequence = 5;
    params.push = true;
    const Bytes pixels{1, 2, 3, 4, 5, 6};

    const Result<Bytes> r = encodeDdp(params, pixels);
    REQUIRE(r.ok());
    const Bytes expected{0x41, 5, 0x0B, 1, 0x01, 0x02, 0x03, 0x04, 0x00, 0x06, 1, 2, 3, 4, 5, 6};
    CHECK(r.value() == expected);

    params.push = false;
    CHECK(encodeDdp(params, pixels).value()[0] == 0x40);
}

TEST_CASE("Ein DDP-Strom wird in Pakete zu 1440 Bytes geschnitten", "[output][ddp]") {
    Bytes pixels(3000);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<std::uint8_t>(i % 251);
    }
    DdpParams params;
    params.offset = 100;
    params.sequence = 3;

    const Result<std::vector<Bytes>> r = encodeDdpStream(params, pixels);
    REQUIRE(r.ok());
    const std::vector<Bytes>& packets = r.value();
    REQUIRE(packets.size() == 3);

    const std::size_t sizes[] = {1440, 1440, 120};
    const std::uint32_t offsets[] = {100, 1540, 2980};
    std::size_t consumed = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        const Bytes& p = packets[i];
        REQUIRE(p.size() == kDdpHeaderSize + sizes[i]);
        CHECK(p[0] == (i == 2 ? 0x41 : 0x40));  // Push nur am letzten
        CHECK(p[1] == 3);
        const std::uint32_t offset = (std::uint32_t{p[4]} << 24U) | (std::uint32_t{p[5]} << 16U) |
                                     (std::uint32_t{p[6]} << 8U) | p[7];
        CHECK(offset == offsets[i]);
        CHECK(((p[8] << 8U) | p[9]) == static_cast<int>(sizes[i]));
        CHECK(Bytes(p.begin() + kDdpHeaderSize, p.end()) ==
              Bytes(pixels.begin() + static_cast<std::ptrdiff_t>(consumed),
                    pixels.begin() + static_cast<std::ptrdiff_t>(consumed + sizes[i])));
        consumed += sizes[i];
    }
}

TEST_CASE("Ohne Push trägt kein Paket des Stroms das Flag", "[output][ddp]") {
    const Bytes pixels(2000, 1);
    DdpParams params;
    params.push = false;
    const Result<std::vector<Bytes>> r = encodeDdpStream(params, pixels);
    REQUIRE(r.ok());
    REQUIRE(r.value().size() == 2);
    CHECK(r.value()[0][0] == 0x40);
    CHECK(r.value()[1][0] == 0x40);
}

TEST_CASE("Genau 1440 Bytes sind ein einziges Paket", "[output][ddp]") {
    const Bytes pixels(1440, 1);
    const Result<std::vector<Bytes>> r = encodeDdpStream({}, pixels);
    REQUIRE(r.ok());
    CHECK(r.value().size() == 1);
    CHECK(r.value()[0][0] == 0x41);
}

TEST_CASE("Ungültige DDP-Eingaben ergeben einen Fehler", "[output][ddp]") {
    const Bytes none;
    const Bytes tooBig(1441, 0);
    const Bytes two(2, 0);
    CHECK(encodeDdp({}, none).error().code == ErrorCode::InvalidArgument);
    CHECK(encodeDdp({}, tooBig).error().code == ErrorCode::InvalidArgument);
    DdpParams badSequence;
    badSequence.sequence = 16;
    CHECK(encodeDdp(badSequence, two).error().code == ErrorCode::InvalidArgument);
    CHECK(encodeDdpStream({}, none).error().code == ErrorCode::InvalidArgument);

    DdpParams edge;
    edge.offset = 0xFFFFFFFEU;
    CHECK(encodeDdpStream(edge, Bytes(1, 0)).ok());
    CHECK(encodeDdpStream(edge, two).error().code == ErrorCode::InvalidArgument);
}
