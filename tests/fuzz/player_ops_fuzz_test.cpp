// player_ops_fuzz_test.cpp: zufällige Bearbeitungsfolgen am Player (Node-Editor ohne Oberfläche).
//
// Warum es den Test gibt: Der Editor ruft Player-Funktionen in beliebiger Reihenfolge auf: Nodes anlegen und löschen
// (mit Überbrücken), Kabel ziehen, Ausgang wählen, Rückgängig, Wiederholen, dazwischen läuft die Wiedergabe. Der Test
// würfelt solche Folgen und prüft nach jedem Schritt die Regeln, die immer gelten müssen: Der Graph ist stimmig (jede
// Verbindung zeigt auf vorhandene, typgleiche Ports, kein Eingang doppelt), der Ausgang zeigt auf einen vorhandenen Node,
// und jede Änderung lässt sich mit einem Rückgängig genau zurücknehmen. Der Zufall ist gesät, ein Fehler ist wiederholbar.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Wand
  matrix 8 4
  patch artnet destination 0 universe 1
end
node g wh.generator.plasma
end
node m wh.transform.mirror
end
node r wh.transform.rotate
end
connect g.out -> m.source
connect m.out -> r.source
output r.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    [[nodiscard]] std::string text() const {
        const auto written = project::writeProject(*player.project());
        return written.ok() ? written.value() : std::string{"<nicht schreibbar>"};
    }
};

/// Die Regeln, die nach jeder Operation gelten müssen. Gibt eine Beschreibung des Verstoßes zurück oder "".
std::string violation(const Rig& rig) {
    const project::Project* project = rig.player.project();
    if (project == nullptr) {
        return "kein Projekt";
    }
    const graph::Graph& graph = project->graph;
    std::set<std::pair<std::uint64_t, std::string>> usedInputs;
    for (const graph::Connection& c : graph.connections()) {
        const auto* from = graph.findNode(c.from.node);
        const auto* to = graph.findNode(c.to.node);
        if (from == nullptr || to == nullptr) {
            return "Verbindung zeigt auf einen fehlenden Node";
        }
        const auto* fromType = graph.registry().find(from->typeId);
        const auto* toType = graph.registry().find(to->typeId);
        const auto* outPort = fromType != nullptr ? fromType->findPort(c.from.portKey) : nullptr;
        const auto* inPort = toType != nullptr ? toType->findPort(c.to.portKey) : nullptr;
        if (outPort == nullptr || inPort == nullptr) {
            return "Verbindung zeigt auf einen fehlenden Port";
        }
        if (outPort->direction != graph::PortDirection::Output || inPort->direction != graph::PortDirection::Input) {
            return "Verbindung läuft in die falsche Richtung";
        }
        if (!usedInputs.insert({c.to.node.value(), c.to.portKey}).second) {
            return "ein Eingang ist doppelt belegt";
        }
        if (c.from.node == c.to.node) {
            return "Verbindung eines Nodes mit sich selbst";
        }
    }
    if (project->target && graph.findNode(project->target->node) == nullptr) {
        return "der Ausgang zeigt auf einen gelöschten Node";
    }
    return {};
}

std::uint64_t seedFromEnvironment(std::uint64_t fallback) {
    const char* text = std::getenv("WH_FUZZ_SEED");
    return text != nullptr ? std::strtoull(text, nullptr, 10) : fallback;
}

}  // namespace

TEST_CASE("Fuzz: zufällige Editor-Operationen halten den Graphen stimmig und sind rückgängig zu machen", "[fuzz][player]") {
    Rig rig;
    std::vector<std::string> typeIds;
    for (const graph::NodeDescriptor* type : rig.library->registry->all()) {
        typeIds.push_back(type->typeId);
    }
    std::mt19937 rng{static_cast<std::uint32_t>(seedFromEnvironment(99))};
    const std::uint64_t rounds = std::getenv("WH_FUZZ_ROUNDS") != nullptr ? std::strtoull(std::getenv("WH_FUZZ_ROUNDS"), nullptr, 10) : 1500;
    bool running = false;

    for (std::uint64_t round = 0; round < rounds; ++round) {
        const project::Project* project = rig.player.project();
        const auto& nodes = project->graph.nodes();
        const std::string before = rig.text();
        const std::size_t op = rng() % 12;
        INFO("Runde " << round << ", Operation " << op << "\nVorher:\n" << before);
        bool changed = false;  // hat die Operation das Projekt verändert (dann muss ein Rückgängig es zurückgeben)?

        switch (op) {
            case 0:
            case 1: {  // Node anlegen
                const auto added = rig.player.addNode(typeIds[rng() % typeIds.size()], Vec2{float(rng() % 500), float(rng() % 500)});
                changed = added.ok();
                break;
            }
            case 2:
            case 3: {  // Node löschen (mit Überbrücken)
                if (nodes.empty()) {
                    break;
                }
                const NodeId victim = nodes[rng() % nodes.size()].id;
                changed = rig.player.removeNode(victim).ok();
                break;
            }
            case 4:
            case 5:
            case 6: {  // zwei zufällige Ports verbinden (Fehler sind erlaubt)
                if (nodes.size() < 2) {
                    break;
                }
                const auto& a = nodes[rng() % nodes.size()];
                const auto& b = nodes[rng() % nodes.size()];
                const auto* ta = rig.library->registry->find(a.typeId);
                const auto* tb = rig.library->registry->find(b.typeId);
                if (ta == nullptr || tb == nullptr || ta->ports.empty() || tb->ports.empty()) {
                    break;
                }
                const auto& pa = ta->ports[rng() % ta->ports.size()];
                const auto& pb = tb->ports[rng() % tb->ports.size()];
                changed = rig.player.connect({a.id, pa.key}, {b.id, pb.key}).ok();
                break;
            }
            case 7: {  // Kabel lösen
                const auto& connections = project->graph.connections();
                if (connections.empty()) {
                    break;
                }
                changed = rig.player.disconnect(connections[rng() % connections.size()].id).ok();
                break;
            }
            case 8: {  // Ausgang wählen
                if (nodes.empty()) {
                    break;
                }
                changed = rig.player.setTarget({nodes[rng() % nodes.size()].id, "out"}).ok();
                break;
            }
            case 9: {  // Rückgängig / Wiederholen als eigene Operation
                if (rng() % 2 == 0 && rig.player.canUndo()) {
                    REQUIRE(rig.player.undo().ok());
                } else if (rig.player.canRedo()) {
                    REQUIRE(rig.player.redo().ok());
                }
                break;
            }
            case 10: {  // Wiedergabe ein/aus und ein paar Frames
                if (running) {
                    rig.player.stop();
                    running = false;
                } else if (rig.player.start().ok()) {
                    running = true;
                }
                break;
            }
            default: break;
        }
        if (running) {
            (void)rig.player.step();
            if (!rig.player.status().running) {
                running = false;  // ein Fehler hat die Wiedergabe beendet: erlaubt
            }
        }

        const std::string broken = violation(rig);
        REQUIRE(broken.empty());
        // Ein Rückgängig nimmt genau diese eine Änderung zurück (auch das Überbrücken beim Löschen).
        if (changed && op != 9 && rig.text() != before) {
            REQUIRE(rig.player.canUndo());
            const std::string after = rig.text();
            REQUIRE(rig.player.undo().ok());
            CHECK(rig.text() == before);
            REQUIRE(rig.player.redo().ok());
            CHECK(rig.text() == after);
        }
    }
    rig.player.stop();
}
