// Tests der Node-Zwischenablage: kopieren, einfügen, Rückgängig, Fehlerfälle.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/node_clipboard.hpp"
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
  set scale 5
  at 100 40
end
node spiegel wh.transform.mirror
  at 360 40
end
node rot wh.transform.rotate
  set angle 33
  at 620 40
end
connect plasma.out -> spiegel.source
connect spiegel.out -> rot.source
output rot.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    [[nodiscard]] NodeId id(const std::string& typeId) const {
        for (const auto& node : player.project()->graph.nodes()) {
            if (node.typeId == typeId) {
                return node.id;
            }
        }
        FAIL("Node nicht gefunden: " << typeId);
        return {};
    }
    [[nodiscard]] std::size_t nodeCount() const { return player.project()->graph.nodes().size(); }
    [[nodiscard]] std::size_t wireCount() const { return player.project()->graph.connections().size(); }
};

}  // namespace

TEST_CASE("Zwischenablage: kopiert Auswahl, Parameter und nur die inneren Kabel", "[clipboard]") {
    Rig rig;
    const std::vector<NodeId> selection{rig.id("wh.generator.plasma"), rig.id("wh.transform.mirror")};
    const runtime::NodeClipboard clip = runtime::copyNodes(rig.player.project()->graph, selection);

    REQUIRE(clip.nodes.size() == 2);
    CHECK(clip.nodes[0].typeId == "wh.generator.plasma");  // Reihenfolge des Graphen
    CHECK(clip.nodes[1].typeId == "wh.transform.mirror");
    REQUIRE(clip.wires.size() == 1);                       // plasma -> spiegel; spiegel -> rot bleibt draußen
    CHECK(clip.wires[0].from == 0);
    CHECK(clip.wires[0].to == 1);
    CHECK(clip.wires[0].toPort == "source");
    CHECK(clip.topLeft() == Vec2{100.0F, 40.0F});

    bool scaleFound = false;
    for (const auto& [key, value] : clip.nodes[0].parameters) {
        if (key == "scale") {
            scaleFound = true;
            CHECK(std::get<double>(value) == 5.0);
        }
    }
    CHECK(scaleFound);
}

TEST_CASE("Zwischenablage: unbekannte Ids ergeben eine leere Kopie", "[clipboard]") {
    Rig rig;
    const std::vector<NodeId> selection{NodeId{9999}};
    CHECK(runtime::copyNodes(rig.player.project()->graph, selection).empty());
    CHECK(runtime::copyNodes(rig.player.project()->graph, {}).empty());
}

TEST_CASE("Zwischenablage: Einfügen legt Nodes, Werte und Kabel an, ein Schritt Rückgängig", "[clipboard]") {
    Rig rig;
    const std::vector<NodeId> selection{rig.id("wh.transform.mirror"), rig.id("wh.transform.rotate")};
    const runtime::NodeClipboard clip = runtime::copyNodes(rig.player.project()->graph, selection);
    const std::size_t nodesBefore = rig.nodeCount();
    const std::size_t wiresBefore = rig.wireCount();
    const auto targetBefore = rig.player.project()->target;
    REQUIRE_FALSE(rig.player.canUndo());

    const auto pasted = rig.player.pasteNodes(clip, Vec2{30.0F, 50.0F});
    REQUIRE(pasted.ok());
    REQUIRE(pasted.value().size() == 2);
    CHECK(rig.nodeCount() == nodesBefore + 2);
    CHECK(rig.wireCount() == wiresBefore + 1);  // das Kabel zwischen den beiden Kopien
    CHECK(rig.player.modified());
    CHECK(rig.player.project()->target == targetBefore);  // der Ausgang bleibt, wo er war

    const graph::NodeInstance* copyOfRotate = rig.player.project()->graph.findNode(pasted.value()[1]);
    REQUIRE(copyOfRotate != nullptr);
    CHECK(copyOfRotate->typeId == "wh.transform.rotate");
    CHECK(copyOfRotate->position == Vec2{650.0F, 90.0F});
    CHECK(std::get<double>(copyOfRotate->parameters.at("angle")) == 33.0);
    CHECK(rig.player.project()->graph.findInputConnection(pasted.value()[1], "source") != nullptr);

    // Ein Schritt zurück nimmt alles auf einmal zurück, und wiederholen bringt es wieder.
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.nodeCount() == nodesBefore);
    CHECK(rig.wireCount() == wiresBefore);
    REQUIRE(rig.player.redo().ok());
    CHECK(rig.nodeCount() == nodesBefore + 2);
}

TEST_CASE("Zwischenablage: mehrfaches Einfügen ergibt unabhängige Kopien", "[clipboard]") {
    Rig rig;
    const std::vector<NodeId> selection{rig.id("wh.generator.plasma")};
    const runtime::NodeClipboard clip = runtime::copyNodes(rig.player.project()->graph, selection);
    const auto a = rig.player.pasteNodes(clip, Vec2{0.0F, 100.0F});
    const auto b = rig.player.pasteNodes(clip, Vec2{0.0F, 200.0F});
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value()[0] != b.value()[0]);
    REQUIRE(rig.player.setParameter(a.value()[0], "scale", 9.0).ok());
    CHECK(std::get<double>(rig.player.project()->graph.findNode(b.value()[0])->parameters.at("scale")) == 5.0);
}

TEST_CASE("Zwischenablage: Fehler ändern nichts", "[clipboard]") {
    Rig rig;
    CHECK(rig.player.pasteNodes({}, Vec2{}).error().code == ErrorCode::InvalidArgument);  // leer

    runtime::NodeClipboard broken;
    broken.nodes.push_back(runtime::ClipboardNode{"wh.generator.plasma", Vec2{}, {}});
    broken.nodes.push_back(runtime::ClipboardNode{"gibt.es.nicht", Vec2{}, {}});
    const std::size_t nodesBefore = rig.nodeCount();
    const auto r = rig.player.pasteNodes(broken, Vec2{});
    REQUIRE_FALSE(r.ok());
    CHECK(rig.nodeCount() == nodesBefore);  // auch der erste, gültige Node ist wieder weg
    CHECK_FALSE(rig.player.canUndo());

    runtime::NodeClipboard badWire;
    badWire.nodes.push_back(runtime::ClipboardNode{"wh.generator.plasma", Vec2{}, {}});
    badWire.wires.push_back(runtime::ClipboardWire{0, "out", 5, "source"});  // Index außerhalb
    CHECK_FALSE(rig.player.pasteNodes(badWire, Vec2{}).ok());
    CHECK(rig.nodeCount() == nodesBefore);
}

TEST_CASE("Zwischenablage: ein eingefügter Ausgangs-Node wird zum Ausgang und legt sich über den bisherigen", "[clipboard][output]") {
    Rig rig;
    const NodeId first = rig.player.addNode("wh.output.canvas", Vec2{0.0F, 0.0F}).value();
    const runtime::NodeClipboard clip = runtime::copyNodes(rig.player.project()->graph, std::vector<NodeId>{first});
    REQUIRE_FALSE(clip.empty());
    const graph::PortRef before = *rig.player.project()->target;
    REQUIRE(before == graph::PortRef{first, "out"});

    const auto pasted = rig.player.pasteNodes(clip, Vec2{0.0F, 200.0F});
    REQUIRE(pasted.ok());
    const NodeId copy = pasted.value().front();
    REQUIRE(rig.player.project()->target.has_value());
    CHECK(*rig.player.project()->target == graph::PortRef{copy, "out"});  // "kein Ausgang" darf nicht vorkommen
    const auto* behind = rig.player.project()->graph.findInputConnection(copy, "behind");
    REQUIRE(behind != nullptr);
    CHECK(behind->from == before);
    CHECK(rig.player.project()->graph.findInputConnection(copy, "source") != nullptr);

    REQUIRE(rig.player.undo().ok());  // der Ausgang kehrt mit dem Einfügen zurück
    CHECK(*rig.player.project()->target == before);
}
