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
destination 1 10.0.0.6

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

namespace {

// Projekt mit einem Look, der einen eigenen wh.value.dmx-Node (Lese-Richtung) UND einen eigenen
// wh.output.dmx-Node (Schreib-Richtung) enthält - beide existieren NUR im gespeicherten Look
// (project_->looks[0].graph), nicht im gerade offenen Hauptgraphen project_->graph. Bug-Reproduktion vom
// 15.11. ("whfx behauptet, kein DMX-Node sei im Projekt, obwohl einer da ist"): der Lese-Node war in genau
// dieser Lage unsichtbar, solange dmxValueNodes() nur project_->graph abgesucht hat;
// dmxOutputValueNodes() braucht dieselbe Behandlung für die Schreib-Richtung.
const char* const kProjectWithLook = R"(whitehole-project 1
destination 0 10.0.0.5

fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end

node g wh.generator.gradient
end
output g.out

look "LookA" fade 1
  node lg wh.generator.gradient
  end
  node ld wh.value.dmx
    set channel 5
  end
  node lo wh.output.dmx
  end
  output lg.out
endlook
)";

struct LookRig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    LookRig() { REQUIRE(player.loadText(kProjectWithLook).ok()); }
};

}  // namespace

TEST_CASE("Player::dmxValueNodes: findet auch Nodes, die nur in einem gespeicherten Look stecken",
          "[player][dmx]") {
    LookRig rig;
    REQUIRE(rig.player.looks().size() == 1);

    const std::vector<runtime::DmxValueNodeRef> nodes = rig.player.dmxValueNodes();
    REQUIRE(nodes.size() == 1);  // project_->graph hat gar keinen wh.value.dmx-Node, nur der Look
    CHECK(nodes[0].lookIndex.has_value());
    REQUIRE(nodes[0].lookIndex.has_value());
    CHECK(*nodes[0].lookIndex == 0);
    CHECK(nodes[0].label.find("LookA") != std::string::npos);  // Label nennt den Look, siehe player_edit.cpp

    // Der Node steht wirklich im Look-Graphen, nicht im Hauptgraphen - dieselbe NodeId findet man in
    // project_->graph gar nicht (andere Graphen, eigener Namensraum für NodeIds).
    CHECK_FALSE(rig.player.dmxListenerAt(DmxProtocol::ArtNet, 0, 5).has_value());
}

TEST_CASE("Player::setLookNodeParameter: schreibt in den Graphen eines gespeicherten Looks, nicht in project_->graph",
          "[player][dmx]") {
    LookRig rig;
    const std::vector<runtime::DmxValueNodeRef> nodes = rig.player.dmxValueNodes();
    REQUIRE(nodes.size() == 1);
    REQUIRE(nodes[0].lookIndex.has_value());
    const std::size_t lookIndex = *nodes[0].lookIndex;
    const NodeId node = nodes[0].id;

    const Result<graph::ParameterValue> set =
        rig.player.setLookNodeParameter(lookIndex, node, "channel", graph::ParameterValue{std::int64_t{9}});
    REQUIRE(set.ok());

    const graph::ParameterValue& stored = rig.player.looks()[lookIndex].graph.findNode(node)->parameters.at("channel");
    REQUIRE(std::holds_alternative<std::int64_t>(stored));
    CHECK(std::get<std::int64_t>(stored) == 9);

    // Ungültiger Look-Index liefert einen Fehler statt zu crashen.
    CHECK_FALSE(rig.player.setLookNodeParameter(42, node, "channel", graph::ParameterValue{std::int64_t{1}}).ok());
}

TEST_CASE("Player::dmxOutputValueNodes: findet auch Nodes, die nur in einem gespeicherten Look stecken",
          "[player][dmx]") {
    LookRig rig;
    const std::vector<runtime::DmxOutputValueNodeRef> nodes = rig.player.dmxOutputValueNodes();
    REQUIRE(nodes.size() == 1);  // project_->graph hat keinen wh.output.dmx-Node, nur der Look
    REQUIRE(nodes[0].lookIndex.has_value());
    CHECK(*nodes[0].lookIndex == 0);
    CHECK(nodes[0].label.find("LookA") != std::string::npos);
    CHECK(nodes[0].poolChannel == 1);  // Standard-Parameter des Nodes (siehe builtin_dmx_out.cpp)
    CHECK(nodes[0].count == 8);
}

TEST_CASE("Player::patchDmxOutputValueNode: legt einen neuen Pool-Patch an oder aktualisiert den bestehenden",
          "[player][dmx]") {
    Rig rig;
    const NodeId node = rig.player.addNode("wh.output.dmx", Vec2{}).value();
    const auto found = rig.player.dmxOutputValueNodes();
    REQUIRE(found.size() == 1);
    CHECK(found[0].id == node);
    const std::uint16_t poolChannel = found[0].poolChannel;
    const std::uint16_t count = found[0].count;

    // Noch kein Patch für diesen Pool-Bereich: ein neuer Eintrag entsteht, destination 0 (Standard).
    REQUIRE(rig.player.patchDmxOutputValueNode(poolChannel, count, output::Protocol::ArtNet, 50, 10).ok());
    REQUIRE(rig.player.project() != nullptr);
    {
        const auto& patches = rig.player.project()->dmxPatches;
        REQUIRE(patches.size() == 1);
        CHECK(patches[0].poolChannel == poolChannel);
        CHECK(patches[0].count == count);
        CHECK(patches[0].protocol == output::Protocol::ArtNet);
        CHECK(patches[0].universe == 50);
        CHECK(patches[0].channel == 10);
        CHECK(patches[0].destination == 0);
    }

    // Eine nicht standardmäßige destination von Hand setzen (kProject kennt zwei, siehe Rig), um zu prüfen,
    // dass ein erneutes Patchen desselben Pool-Bereichs sie nicht stillschweigend zurücksetzt.
    {
        std::vector<output::DmxChannelPatch> withDestination = rig.player.project()->dmxPatches;
        withDestination[0].destination = 1;
        REQUIRE(rig.player.setDmxPatches(withDestination).ok());
    }

    // Derselbe Pool-Bereich erneut gepatcht (anderes Universe/Kanal): aktualisiert den bestehenden Eintrag,
    // statt einen zweiten anzulegen - destination bleibt erhalten.
    REQUIRE(rig.player.patchDmxOutputValueNode(poolChannel, count, output::Protocol::Sacn, 60, 20).ok());
    const auto& patches = rig.player.project()->dmxPatches;
    REQUIRE(patches.size() == 1);
    CHECK(patches[0].protocol == output::Protocol::Sacn);
    CHECK(patches[0].universe == 60);
    CHECK(patches[0].channel == 20);
    CHECK(patches[0].destination == 1);
}
