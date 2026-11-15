// findDmxOutputUsage: wer einen gesendeten DMX-Kanal belegt (Fixture-Patch oder DMX-Kanal-Pool).
#include <catch2/catch_test_macros.hpp>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/runtime/dmx_output_usage.hpp"

using namespace whitehole;
using whitehole::output::DmxChannelPatch;
using whitehole::output::FixturePatch;
using whitehole::output::Protocol;
using whitehole::runtime::DmxOutputOccupant;
using whitehole::runtime::findDmxOutputOccupant;

namespace {

mapping::StripShape strip(std::uint32_t count) {
    mapping::StripShape s;
    s.count = count;
    s.end = Vec3{static_cast<float>(count), 0.0F, 0.0F};
    return s;
}

}  // namespace

TEST_CASE("findDmxOutputOccupant: Fixture-Patch wird gefunden, auch im Folge-Universe", "[runtime][dmx]") {
    mapping::Layout layout;
    const auto id = layout.add("Strip", Vec3{}, strip(200));  // 200 LEDs * 3 Kanäle = 600 > 510 -> 2. Universe
    REQUIRE(id.ok());

    FixturePatch patch;
    patch.fixture = id.value();
    patch.protocol = Protocol::ArtNet;
    patch.universe = 5;
    patch.channel = 1;
    std::vector<FixturePatch> patches{patch};
    std::vector<DmxChannelPatch> dmxPatches;

    // Kanal 1 im ersten Universe: gehört zum Fixture.
    const auto first = findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::ArtNet, 5, 1);
    REQUIRE(first.has_value());
    CHECK(first->kind == DmxOutputOccupant::Kind::Fixture);
    CHECK(first->name == "Strip");

    // 170 LEDs (510 Kanäle) passen ins erste Universe; die restlichen 30 LEDs (90 Kanäle) landen in Universe 6.
    const auto second = findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::ArtNet, 6, 1);
    REQUIRE(second.has_value());
    CHECK(second->name == "Strip");

    // Kein Fixture belegt Universe 7.
    CHECK_FALSE(findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::ArtNet, 7, 1).has_value());
    // sACN auf demselben Universe/Kanal ist ein anderes Protokoll, nicht betroffen.
    CHECK_FALSE(findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::Sacn, 5, 1).has_value());
}

TEST_CASE("findDmxOutputOccupant: DDP-Patches werden nie als Treffer gemeldet", "[runtime][dmx]") {
    mapping::Layout layout;
    const auto id = layout.add("Strip", Vec3{}, strip(10));
    REQUIRE(id.ok());

    FixturePatch patch;
    patch.fixture = id.value();
    patch.protocol = Protocol::Ddp;
    patch.ddpOffset = 0;
    std::vector<FixturePatch> patches{patch};
    std::vector<DmxChannelPatch> dmxPatches;

    CHECK_FALSE(findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::ArtNet, 0, 1).has_value());
}

TEST_CASE("findDmxOutputOccupant: Kanal-Pool-Patch wird gefunden", "[runtime][dmx]") {
    mapping::Layout layout;  // keine Fixtures nötig, der Pool ist unabhängig davon

    DmxChannelPatch patch;
    patch.poolChannel = 1;
    patch.count = 10;
    patch.protocol = Protocol::Sacn;
    patch.universe = 9;
    patch.channel = 50;
    std::vector<FixturePatch> patches;
    std::vector<DmxChannelPatch> dmxPatches{patch};

    const auto hit = findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::Sacn, 9, 55);
    REQUIRE(hit.has_value());
    CHECK(hit->kind == DmxOutputOccupant::Kind::Pool);
    CHECK(hit->poolChannel == 1);

    CHECK_FALSE(findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::Sacn, 9, 60).has_value());
    CHECK_FALSE(findDmxOutputOccupant(layout, patches, dmxPatches, Protocol::ArtNet, 9, 55).has_value());
}
