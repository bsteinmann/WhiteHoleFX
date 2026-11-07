// Tests: Patch auf das Protokoll seines Controllers umstellen (Art-Net zählt ab 0, sACN ab 1).
#include <catch2/catch_test_macros.hpp>

#include "whitehole/output/patch_protocol.hpp"

using namespace whitehole;
using output::Protocol;

TEST_CASE("Patch-Protokoll: Art-Net -> sACN verschiebt das Universe um +1 und zurück um -1", "[output][protocol]") {
    output::FixturePatch patch;
    patch.protocol = Protocol::ArtNet;
    patch.universe = 0;
    auto shifted = output::alignPatchProtocol(patch, Protocol::Sacn);
    REQUIRE(shifted.ok());
    CHECK(shifted.value() == 1);
    CHECK(patch.protocol == Protocol::Sacn);
    CHECK(patch.universe == 1);

    shifted = output::alignPatchProtocol(patch, Protocol::ArtNet);
    REQUIRE(shifted.ok());
    CHECK(shifted.value() == -1);
    CHECK(patch.universe == 0);
}

TEST_CASE("Patch-Protokoll: gleiches Protokoll ändert nichts, DDP lässt die Nummer stehen", "[output][protocol]") {
    output::FixturePatch patch;
    patch.protocol = Protocol::Sacn;
    patch.universe = 7;
    REQUIRE(output::alignPatchProtocol(patch, Protocol::Sacn).value() == 0);
    CHECK(patch.universe == 7);

    REQUIRE(output::alignPatchProtocol(patch, Protocol::Ddp).value() == 0);
    CHECK(patch.protocol == Protocol::Ddp);
    CHECK(patch.universe == 7);

    patch.universe = 0;  // von DDP nach sACN: kleinste gültige Nummer
    REQUIRE(output::alignPatchProtocol(patch, Protocol::Sacn).ok());
    CHECK(patch.universe == 1);
}

TEST_CASE("Patch-Protokoll: sACN-Universe außerhalb von Art-Net wird abgelehnt und bleibt unverändert", "[output][protocol]") {
    output::FixturePatch patch;
    patch.protocol = Protocol::Sacn;
    patch.universe = 40000;
    const auto result = output::alignPatchProtocol(patch, Protocol::ArtNet);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::InvalidArgument);
    CHECK(patch.protocol == Protocol::Sacn);
    CHECK(patch.universe == 40000);
}
