// Tests für Player::setDmxPatches (dmxpatch-Einträge, interner DMX-Kanal-Pool auf echte Ziele
// abgebildet). Eigene Datei statt player_layout_test.cpp zu erweitern, die schon nahe am ~400-Zeilen-Limit
// ist (siehe CLAUDE.md).
#include <catch2/catch_test_macros.hpp>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node g wh.generator.gradient
end
output g.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }
};

output::DmxChannelPatch poolPatch(std::uint16_t poolChannel, std::uint16_t count, std::uint16_t universe,
                                   std::uint16_t channel = 1, std::uint32_t destination = 0) {
    output::DmxChannelPatch p;
    p.poolChannel = poolChannel;
    p.count = count;
    p.universe = universe;
    p.channel = channel;
    p.destination = destination;
    return p;
}

}  // namespace

TEST_CASE("DMX-Kanal-Pool: Patch anlegen", "[player][dmxpatch]") {
    Rig rig;
    REQUIRE(rig.player.setDmxPatches({poolPatch(1, 8, 2)}).ok());
    CHECK(rig.player.project()->dmxPatches.size() == 1);
    CHECK(rig.player.project()->dmxPatches.front().poolChannel == 1);
    CHECK(rig.player.project()->dmxPatches.front().universe == 2);
    CHECK(rig.player.modified());
}

TEST_CASE("DMX-Kanal-Pool: unbekanntes Ziel wird abgelehnt, Projekt bleibt unverändert", "[player][dmxpatch]") {
    Rig rig;
    const Result<void> r = rig.player.setDmxPatches({poolPatch(1, 4, 2, 1, 7)});
    CHECK_FALSE(r.ok());
    CHECK(r.error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.project()->dmxPatches.empty());
    CHECK_FALSE(rig.player.modified());
}

TEST_CASE("DMX-Kanal-Pool: DDP-Ziel wird abgelehnt", "[player][dmxpatch]") {
    Rig rig;
    REQUIRE(rig.player.setDestinations({{"10.0.0.5", 0}, {"10.0.0.9", 4048, output::Protocol::Ddp}}).ok());
    output::DmxChannelPatch p = poolPatch(1, 4, 0, 1, 1);
    p.protocol = output::Protocol::Ddp;
    CHECK_FALSE(rig.player.setDmxPatches({p}).ok());
    CHECK(rig.player.project()->dmxPatches.empty());
}

TEST_CASE("DMX-Kanal-Pool: Universe außerhalb des Bereichs wird abgelehnt", "[player][dmxpatch]") {
    Rig rig;
    output::DmxChannelPatch p = poolPatch(1, 4, 0);
    p.protocol = output::Protocol::Sacn;  // sACN: Universe beginnt bei 1, 0 ist ungültig
    CHECK_FALSE(rig.player.setDmxPatches({p}).ok());
    CHECK(rig.player.project()->dmxPatches.empty());
}

TEST_CASE("DMX-Kanal-Pool: Kanal außerhalb des Bereichs wird abgelehnt", "[player][dmxpatch]") {
    Rig rig;
    CHECK_FALSE(rig.player.setDmxPatches({poolPatch(1, 4, 1, 510)}).ok());  // 510 + 4 - 1 > 512
    CHECK(rig.player.project()->dmxPatches.empty());
}

TEST_CASE("DMX-Kanal-Pool: interner Kanal außerhalb des Bereichs wird abgelehnt", "[player][dmxpatch]") {
    Rig rig;
    CHECK_FALSE(rig.player.setDmxPatches({poolPatch(510, 4, 1)}).ok());  // 510 + 4 - 1 > 512
    CHECK(rig.player.project()->dmxPatches.empty());
}

TEST_CASE("DMX-Kanal-Pool: überschneidende Pool-Bereiche werden abgelehnt", "[player][dmxpatch]") {
    Rig rig;
    const Result<void> r = rig.player.setDmxPatches({poolPatch(1, 8, 1), poolPatch(5, 4, 1, 20)});
    CHECK_FALSE(r.ok());
    CHECK(r.error().code == ErrorCode::AlreadyExists);
    CHECK(rig.player.project()->dmxPatches.empty());
}

TEST_CASE("DMX-Kanal-Pool: mehrere nicht überschneidende Patches sind erlaubt", "[player][dmxpatch]") {
    Rig rig;
    REQUIRE(rig.player.setDmxPatches({poolPatch(1, 8, 1), poolPatch(9, 4, 1, 20)}).ok());
    CHECK(rig.player.project()->dmxPatches.size() == 2);
}

TEST_CASE("DMX-Kanal-Pool: ersetzen überschreibt die vorherige Liste", "[player][dmxpatch]") {
    Rig rig;
    REQUIRE(rig.player.setDmxPatches({poolPatch(1, 8, 1)}).ok());
    REQUIRE(rig.player.setDmxPatches({poolPatch(1, 4, 1), poolPatch(5, 4, 1, 10)}).ok());
    CHECK(rig.player.project()->dmxPatches.size() == 2);
}
