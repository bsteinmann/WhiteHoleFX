// node_display_name_test.cpp: Automatikname plus eigener Name in Klammern (node_display_name.hpp).
//
// Der eigene Name (NodeInstance::label) soll zusätzlich zum Automatiknamen stehen, nicht ihn ersetzen (vom
// Nutzer gewünscht) - so bleibt auch mit eigenem Namen auf einen Blick sichtbar, was für ein Node das ist.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/node_display_name.hpp"
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
node plasma wh.generator.plasma
end
node spiegel1 wh.transform.mirror
end
node spiegel2 wh.transform.mirror
end
output spiegel1.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    // Die Namen hinter "node" in der Projektdatei sind nur lokale Bezeichner für "connect"/"output" beim
    // Parsen, kein NodeInstance::label - Reihenfolge der Deklaration zählt (plasma, spiegel1, spiegel2).
    [[nodiscard]] NodeId plasma() const { return player.project()->graph.nodes()[0].id; }
    [[nodiscard]] NodeId spiegel1() const { return player.project()->graph.nodes()[1].id; }
    [[nodiscard]] NodeId spiegel2() const { return player.project()->graph.nodes()[2].id; }
};

}  // namespace

TEST_CASE("node_display_name: ohne eigenen Namen bleibt es beim Automatiknamen, durchnummeriert", "[runtime][node-name]") {
    Rig rig;
    const auto& g = rig.player.project()->graph;
    CHECK(runtime::nodeDisplayName(g, rig.plasma()) == "Plasma");  // einziger seines Typs: keine Nummer
    CHECK(runtime::nodeDisplayName(g, rig.spiegel1()) == "Mirror 1");
    CHECK(runtime::nodeDisplayName(g, rig.spiegel2()) == "Mirror 2");
}

TEST_CASE("node_display_name: eigener Name steht zusätzlich zum Automatiknamen, nicht statt ihm",
          "[runtime][node-name]") {
    Rig rig;
    const NodeId spiegel1 = rig.spiegel1();
    REQUIRE(rig.player.renameNode(spiegel1, "Eingangstür").ok());
    const auto& g = rig.player.project()->graph;
    CHECK(runtime::nodeDisplayName(g, spiegel1) == "Mirror 1 (Eingangstür)");
    // Der andere Node desselben Typs bleibt unverändert nummeriert - das Umbenennen des einen wirkt sich
    // nicht auf die Nummerierung des anderen aus.
    CHECK(runtime::nodeDisplayName(g, rig.spiegel2()) == "Mirror 2");
}
