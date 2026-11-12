// Adressraum eines Patches: Anzeige (addressSpan), belegte Bereiche (channelBlocks) und freier Platz (fitChannel).
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "whitehole/output/patch_space.hpp"

using namespace whitehole;
using namespace whitehole::output;

namespace {

FixturePatch patchAt(std::uint16_t universe, std::uint16_t channel, Protocol protocol = Protocol::Sacn) {
    FixturePatch patch;
    patch.protocol = protocol;
    patch.universe = universe;
    patch.channel = channel;
    return patch;
}

}  // namespace

TEST_CASE("Adressraum: eine Matrix im ersten Universe", "[output][patch_space]") {
    // 9 x 11 = 99 LEDs = 297 Kanäle
    const auto span = addressSpan(patchAt(1, 1), 99);
    REQUIRE(span.has_value());
    CHECK(span->firstUniverse == 1);
    CHECK(span->firstChannel == 1);
    CHECK(span->lastUniverse == 1);
    CHECK(span->lastChannel == 297);
}

TEST_CASE("Adressraum: ein langer Strip läuft ins nächste Universe", "[output][patch_space]") {
    // 400 LEDs ab Kanal 1: 170 in U1, 170 in U2, 60 in U3 (Kanal 1..180)
    const auto span = addressSpan(patchAt(1, 1), 400);
    REQUIRE(span.has_value());
    CHECK(span->lastUniverse == 3);
    CHECK(span->lastChannel == 180);
    // Start mitten im Universe: ab Kanal 298 passen noch 71 LEDs (298..510), der Rest geht in U2 weiter
    const auto shifted = addressSpan(patchAt(1, 298), 100);
    REQUIRE(shifted.has_value());
    CHECK(shifted->firstChannel == 298);
    CHECK(shifted->lastUniverse == 2);
    CHECK(shifted->lastChannel == 29 * 3);
}

TEST_CASE("Adressraum: DDP nennt Bytes, 0 LEDs nichts", "[output][patch_space]") {
    FixturePatch ddp = patchAt(0, 1, Protocol::Ddp);
    ddp.ddpOffset = 30;
    const auto span = addressSpan(ddp, 10);
    REQUIRE(span.has_value());
    CHECK(span->firstChannel == 30);
    CHECK(span->lastChannel == 59);
    CHECK_FALSE(addressSpan(patchAt(1, 1), 0).has_value());
    CHECK(channelBlocks(ddp, 10).empty());
}

TEST_CASE("fitChannel: der Pixel hinter der Matrix im selben Universe", "[output][patch_space]") {
    const auto matrix = channelBlocks(patchAt(1, 1), 99);
    // Wunsch: Universe 1, Kanal 1 ist belegt -> Kanal 298 (direkt hinter der Matrix)
    CHECK(fitChannel(patchAt(1, 1), 1, matrix) == 298);
    // Wunschkanal frei: bleibt
    CHECK(fitChannel(patchAt(1, 400), 1, matrix) == 400);
    // anderes Universe oder anderes Ziel: bleibt
    CHECK(fitChannel(patchAt(2, 1), 1, matrix) == 1);
    FixturePatch other = patchAt(1, 1);
    other.destination = 1;
    CHECK(fitChannel(other, 1, matrix) == 1);
}

TEST_CASE("fitChannel: kein Platz mehr", "[output][patch_space]") {
    // 170 LEDs füllen das Universe: ein weiteres Fixture passt dort nirgends
    const auto full = channelBlocks(patchAt(1, 1), 170);
    CHECK_FALSE(fitChannel(patchAt(1, 1), 1, full).has_value());
    // Hinter der Matrix passt ein Fixture, das weiterläuft, nur wenn das nächste Universe frei ist
    const auto two = channelBlocks(patchAt(1, 1), 99);
    const auto blocked = std::vector<ChannelBlock>{two.front(), channelBlocks(patchAt(2, 1), 1).front()};
    CHECK_FALSE(fitChannel(patchAt(1, 1), 100, blocked).has_value());
    CHECK(fitChannel(patchAt(1, 1), 100, two) == 298);
}

TEST_CASE("Adressraum: übersprungene Pixel belegen ihre Kanäle mit", "[output][patch_space][skip]") {
    FixturePatch patch = patchAt(1, 1);
    patch.skipFirst = 2;
    patch.skipEvery = 5;
    patch.skipCount = 1;
    // 10 LEDs: 2 + 10 + 1 (nach LED 5; nach LED 10 kommt nichts mehr) = 13 Plätze = 39 Kanäle
    const auto span = addressSpan(patch, 10);
    REQUIRE(span.has_value());
    CHECK(span->firstChannel == 1);
    CHECK(span->lastChannel == 39);
    const std::vector<ChannelBlock> blocks = channelBlocks(patch, 10);
    REQUIRE(blocks.size() == 1);
    CHECK(blocks[0].endByte == 39);
}
