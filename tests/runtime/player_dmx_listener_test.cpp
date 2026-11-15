// Player::dmxListenerAt: Graph-Nodes zusätzlich zu Master-Helligkeit/Live-DMX, Schritt 4 des
// DMX-Patch/Monitor-Gitters ("Wer hört zu"-Overlay, siehe CLAUDE.md). Prüft auch DmxInputListener::Kind
// (nachträglich freigegebene Erweiterung, Grundlage für die Zellenfarben im DMX-Monitor).
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::input::DmxProtocol;
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

}  // namespace

TEST_CASE("Player::dmxListenerAt: Master-Helligkeit und Live-DMX werden als Hörer gemeldet", "[player][dmx]") {
    Rig rig;
    CHECK_FALSE(rig.player.dmxListenerAt(DmxProtocol::ArtNet, 2, 3).has_value());

    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::ArtNet, 2, 3});
    const auto master = rig.player.dmxListenerAt(DmxProtocol::ArtNet, 2, 3);
    REQUIRE(master.has_value());
    CHECK(master->name == "Master-Helligkeit");
    CHECK(master->kind == runtime::DmxInputListener::Kind::Master);
    CHECK_FALSE(rig.player.dmxListenerAt(DmxProtocol::ArtNet, 2, 4).has_value());  // Nachbarkanal, nicht betroffen

    rig.player.setLiveDmx(runtime::LiveDmxSource{DmxProtocol::Sacn, 9, 1, 2});
    const auto select = rig.player.dmxListenerAt(DmxProtocol::Sacn, 9, 1);
    REQUIRE(select.has_value());
    CHECK(select->name == "Live-Look-Auswahl");
    CHECK(select->kind == runtime::DmxInputListener::Kind::LiveSelect);
    const auto fade = rig.player.dmxListenerAt(DmxProtocol::Sacn, 9, 2);
    REQUIRE(fade.has_value());
    CHECK(fade->name == "Live-Überblendzeit");
    CHECK(fade->kind == runtime::DmxInputListener::Kind::LiveFade);
}

TEST_CASE("Player::dmxListenerAt: ein Node im Graphen geht vor Master/Live, falls sie sich überschneiden", "[player][dmx]") {
    Rig rig;
    const NodeId node = rig.player.addNode("wh.value.dmx", Vec2{}).value();
    REQUIRE(rig.player.setParameter(node, "channel", graph::ParameterValue{std::int64_t{3}}).ok());
    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::ArtNet, 1, 3});  // derselbe Kanal wie der Node

    const auto found = rig.player.dmxListenerAt(DmxProtocol::ArtNet, 1, 3);
    REQUIRE(found.has_value());
    CHECK(found->name == "DMX Channel");
    CHECK(found->kind == runtime::DmxInputListener::Kind::Node);
}
