// Output-Node duplizieren (ABSTUERZE_TODO.md Punkt 9): Das Bild darf dabei nicht schwarz werden. Warum eine eigene Datei:
// node_clipboard_test.cpp prüft das Kopieren allgemein; hier geht es nur um den Sonderfall "Output-Node einfügen" und
// darum, was danach als Ausgang des Projekts zählt (Player::pasteNodes) und was die Engine daraus macht (mergeOutputs).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/graph/output_merge.hpp"
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
  at 100 40
end
node ausgang wh.output.canvas
  at 360 40
end
connect plasma.out -> ausgang.source
output ausgang.out
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
};

/// Wie viele Nodes hängen (über beliebig viele Kabel) vor `target`? Ein Maß dafür, ob der Ausgang Bild liefert.
bool reachesGenerator(const graph::Graph& graph, const graph::PortRef& target) {
    std::vector<NodeId> open{target.node};
    std::vector<NodeId> seen;
    while (!open.empty()) {
        const NodeId current = open.back();
        open.pop_back();
        if (std::find(seen.begin(), seen.end(), current) != seen.end()) {
            continue;
        }
        seen.push_back(current);
        if (const graph::NodeInstance* node = graph.findNode(current); node != nullptr && node->typeId == "wh.generator.plasma") {
            return true;
        }
        for (const graph::Connection& wire : graph.connections()) {
            if (wire.to.node == current) {
                open.push_back(wire.from.node);
            }
        }
    }
    return false;
}

/// Was die Engine tatsächlich rendert: der Ausgang nach dem unsichtbaren Zusammenfassen mehrerer Outputs.
graph::PortRef effectiveTarget(const runtime::Player& player) {
    const graph::Graph& graph = player.project()->graph;
    const graph::PortRef target = *player.project()->target;
    if (const auto merged = graph::mergeOutputs(graph, target)) {
        return merged->target;
    }
    return target;
}

}  // namespace

TEST_CASE("Output duplizieren: das Bild bleibt erhalten, der bisherige Ausgang bleibt der Ausgang", "[clipboard][output]") {
    Rig rig;
    const NodeId original = rig.id("wh.output.canvas");
    REQUIRE(reachesGenerator(rig.player.project()->graph, effectiveTarget(rig.player)));

    const std::vector<NodeId> selection{original};
    const auto pasted = rig.player.pasteNodes(runtime::copyNodes(rig.player.project()->graph, selection), Vec2{40.0F, 40.0F});
    REQUIRE(pasted.ok());
    REQUIRE(pasted.value().size() == 1);

    // Variante (a), vom Nutzer gewählt: Die Kopie ist ein zusätzlicher, unverbundener Node; der Ausgang bleibt am Original.
    REQUIRE(rig.player.project()->target.has_value());
    CHECK(rig.player.project()->target->node == original);
    CHECK(rig.player.project()->graph.findInputConnection(pasted.value()[0], "source") == nullptr);
    CHECK(reachesGenerator(rig.player.project()->graph, effectiveTarget(rig.player)));  // nicht schwarz

    // Rückgängig stellt den alten Zustand wieder her.
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->target->node == original);
}

TEST_CASE("Output einfügen: ohne bisherigen Ausgang wird die Kopie der Ausgang", "[clipboard][output]") {
    Rig source;  // liefert nur die Kopie eines Output-Nodes
    const runtime::NodeClipboard clip =
        runtime::copyNodes(source.player.project()->graph, std::vector<NodeId>{source.id("wh.output.canvas")});

    // Altes Projekt ohne Output-Node und ohne Ausgang
    Rig other;
    REQUIRE(other.player.loadText(R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
  at 100 40
end
)").ok());
    REQUIRE_FALSE(other.player.project()->target.has_value());

    const auto pasted = other.player.pasteNodes(clip, Vec2{0.0F, 100.0F});
    REQUIRE(pasted.ok());
    REQUIRE(other.player.project()->target.has_value());
    CHECK(other.player.project()->target->node == pasted.value()[0]);  // sonst meldet das Projekt "kein Ausgang"
}
