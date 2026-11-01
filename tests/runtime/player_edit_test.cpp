// Tests der Graph-Bearbeitung im Player (Grundlage des Node-Editors).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

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
node plasma wh.generator.plasma
end
node spiegel wh.transform.mirror
end
node lfo wh.value.lfo
end
node rot wh.transform.rotate
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
    [[nodiscard]] std::size_t connections() const { return player.project()->graph.connections().size(); }
};

}  // namespace

TEST_CASE("Player: geladene Dateien ohne Positionen werden angeordnet", "[player][edit]") {
    Rig rig;
    const auto& g = rig.player.project()->graph;
    CHECK(g.findNode(rig.id("wh.generator.plasma"))->position.x == 0.0F);
    CHECK(g.findNode(rig.id("wh.transform.rotate"))->position.x == 520.0F);
    CHECK_FALSE(rig.player.modified());  // die Anordnung zählt nicht als Änderung des Nutzers
}

TEST_CASE("Player: Node hinzufügen und entfernen", "[player][edit]") {
    Rig rig;
    const std::size_t before = rig.player.project()->graph.nodes().size();
    const auto added = rig.player.addNode("wh.generator.noise", Vec2{10.0F, 20.0F});
    REQUIRE(added.ok());
    CHECK(rig.player.project()->graph.nodes().size() == before + 1);
    CHECK(rig.player.project()->graph.findNode(added.value())->position == Vec2{10.0F, 20.0F});
    CHECK(rig.player.modified());

    CHECK(rig.player.addNode("gibt.es.nicht", Vec2{}).error().code == ErrorCode::NotFound);
    CHECK(rig.player.removeNode(added.value()).ok());
    CHECK(rig.player.project()->graph.nodes().size() == before);
    CHECK(rig.player.removeNode(added.value()).error().code == ErrorCode::NotFound);
}

TEST_CASE("Player: Löschen des Ausgangs-Nodes rückt den Ausgang auf den Vorgänger", "[player][edit]") {
    Rig rig;
    const NodeId mirror = rig.id("wh.transform.mirror");
    const NodeId plasma = rig.id("wh.generator.plasma");
    const NodeId rotate = rig.id("wh.transform.rotate");

    REQUIRE(rig.player.removeNode(rotate).ok());
    REQUIRE(rig.player.project()->target.has_value());
    CHECK(rig.player.project()->target->node == mirror);
    CHECK(rig.player.project()->target->portKey == "out");

    REQUIRE(rig.player.removeNode(mirror).ok());
    REQUIRE(rig.player.project()->target.has_value());
    CHECK(rig.player.project()->target->node == plasma);

    // Ganz am Anfang der Kette gibt es keinen Vorgänger mehr: das Projekt hat dann keinen Ausgang.
    REQUIRE(rig.player.removeNode(plasma).ok());
    CHECK_FALSE(rig.player.project()->target.has_value());
    CHECK(rig.player.start().error().code == ErrorCode::InvalidArgument);

    // Alles lässt sich zurücknehmen, auch der Ausgang.
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->target->node == plasma);
    REQUIRE(rig.player.undo().ok());
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->target->node == rotate);
    CHECK(rig.player.project()->graph.findNode(rotate) != nullptr);
}

TEST_CASE("Player: Ausgang löschen im Lauf startet die Session mit dem neuen Ausgang", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    REQUIRE(rig.player.removeNode(rig.id("wh.transform.rotate")).ok());
    CHECK(rig.player.step().ok());
    CHECK(rig.player.status().running);
}

TEST_CASE("Player: Node entfernen löst seine Verbindungen", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.connections() == 2);
    REQUIRE(rig.player.removeNode(rig.id("wh.transform.mirror")).ok());
    // Die Nachbarn werden direkt verbunden (plasma -> rot), statt die Kette zu zerreißen.
    CHECK(rig.connections() == 1);
}

TEST_CASE("Player: Löschen in der Mitte verbindet die Nachbarn neu", "[player][edit]") {
    Rig rig;
    const NodeId plasma = rig.id("wh.generator.plasma");
    const NodeId rotate = rig.id("wh.transform.rotate");
    REQUIRE(rig.player.removeNode(rig.id("wh.transform.mirror")).ok());
    const auto& connections = rig.player.project()->graph.connections();
    REQUIRE(connections.size() == 1);
    CHECK(connections[0].from.node == plasma);
    CHECK(connections[0].to.node == rotate);
    CHECK(connections[0].to.portKey == "source");

    // Eine einzige Rücknahme stellt die ganze Kette wieder her.
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.connections() == 2);
}

TEST_CASE("Player: Löschen am Ende der Kette hinterlässt keine lose Verbindung", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.player.removeNode(rig.id("wh.generator.plasma")).ok());
    CHECK(rig.connections() == 1);  // spiegel -> rot bleibt
}

TEST_CASE("Player: fehlt dem Ausgang der Eingang, bleiben die LEDs nicht auf dem letzten Look", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    CHECK_FALSE(rig.player.outputIncomplete());

    // Die Verbindung in den Ausgangs-Node kappen: der Effekt ist unvollständig.
    ConnectionId toRotate{};
    for (const auto& c : rig.player.project()->graph.connections()) {
        if (c.to.node == rig.id("wh.transform.rotate")) {
            toRotate = c.id;
        }
    }
    REQUIRE(rig.player.disconnect(toRotate).ok());
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.outputIncomplete());
    CHECK(rig.player.status().running);

    // Wieder verbunden: der Effekt läuft wieder.
    REQUIRE(rig.player.connect({rig.id("wh.transform.mirror"), "out"}, {rig.id("wh.transform.rotate"), "source"}).ok());
    REQUIRE(rig.player.step().ok());
    CHECK_FALSE(rig.player.outputIncomplete());
}

TEST_CASE("Player: eine Auswahl-Ganzzahl meldet ihre Namen (LFO-Form als Dropdown)", "[player][edit]") {
    Rig rig;
    bool found = false;
    for (const runtime::NodeInfo& node : rig.player.nodes()) {
        if (node.typeId != "wh.value.lfo") {
            continue;
        }
        for (const runtime::ParameterInfo& parameter : node.parameters) {
            if (parameter.key == "shape") {
                found = true;
                REQUIRE(parameter.choices.size() == 4);
                CHECK(parameter.choices.front() == "Sine");
                CHECK(parameter.choices.back() == "Square");
                CHECK(parameter.maxValue == 3.0);
            } else {
                CHECK(parameter.choices.empty());
            }
        }
    }
    CHECK(found);
}

TEST_CASE("Player: verbinden ersetzt einen belegten Eingang", "[player][edit]") {
    Rig rig;
    const NodeId lfo = rig.id("wh.value.lfo");
    const NodeId rot = rig.id("wh.transform.rotate");
    const NodeId plasma = rig.id("wh.generator.plasma");
    REQUIRE(rig.player.connect({lfo, "out"}, {rot, "angle"}).ok());
    CHECK(rig.connections() == 3);

    // plasma statt spiegel an rot.source: die alte Verbindung weicht
    REQUIRE(rig.player.connect({plasma, "out"}, {rot, "source"}).ok());
    CHECK(rig.connections() == 3);
    const auto* c = rig.player.project()->graph.findInputConnection(rot, "source");
    REQUIRE(c != nullptr);
    CHECK(c->from.node == plasma);
}

TEST_CASE("Player: eine abgelehnte Verbindung lässt die alte stehen", "[player][edit]") {
    Rig rig;
    const NodeId plasma = rig.id("wh.generator.plasma");
    const NodeId mirror = rig.id("wh.transform.mirror");
    const NodeId rot = rig.id("wh.transform.rotate");
    // rot.out -> mirror.source würde einen Zyklus bilden; mirror.source ist belegt
    const auto r = rig.player.connect({rot, "out"}, {mirror, "source"});
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == ErrorCode::CycleDetected);
    const auto* c = rig.player.project()->graph.findInputConnection(mirror, "source");
    REQUIRE(c != nullptr);
    CHECK(c->from.node == plasma);
    CHECK(rig.connections() == 2);
}

TEST_CASE("Player: trennen", "[player][edit]") {
    Rig rig;
    const auto* c = rig.player.project()->graph.findInputConnection(rig.id("wh.transform.mirror"), "source");
    REQUIRE(c != nullptr);
    CHECK(rig.player.disconnect(c->id).ok());
    CHECK(rig.connections() == 1);
    CHECK(rig.player.disconnect(ConnectionId{999}).error().code == ErrorCode::NotFound);
}

TEST_CASE("Player: moveNode ändert die Lage, nicht den Effekt", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    const std::size_t pipelines = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.moveNode(rig.id("wh.transform.mirror"), Vec2{1.0F, 2.0F}).ok());
    CHECK(rig.player.modified());
    REQUIRE(rig.player.step().ok());
    CHECK(rig.device.pipelineDescs().size() == pipelines);
}

TEST_CASE("Player: setTarget nimmt nur Field-Ausgänge", "[player][edit]") {
    Rig rig;
    const NodeId lfo = rig.id("wh.value.lfo");
    const NodeId mirror = rig.id("wh.transform.mirror");
    CHECK(rig.player.setTarget({lfo, "out"}).error().code == ErrorCode::IncompatiblePorts);
    CHECK(rig.player.setTarget({mirror, "gibtsnicht"}).error().code == ErrorCode::NotFound);
    CHECK(rig.player.setTarget({NodeId{999}, "out"}).error().code == ErrorCode::NotFound);
    CHECK(rig.player.setTarget({mirror, "source"}).error().code == ErrorCode::IncompatiblePorts);  // Eingang
    REQUIRE(rig.player.setTarget({mirror, "out"}).ok());
    CHECK(rig.player.project()->target->node == mirror);
    // jetzt ist rot nicht mehr der Ausgang und darf gehen
    CHECK(rig.player.removeNode(rig.id("wh.transform.rotate")).ok());
}

TEST_CASE("Player: Graphänderungen im Lauf werden im nächsten Frame übernommen", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    const std::size_t pipelines = rig.device.pipelineDescs().size();

    // Spiegel überbrücken: Struktur ändert sich, also neue Pipeline
    const NodeId plasma = rig.id("wh.generator.plasma");
    REQUIRE(rig.player.connect({plasma, "out"}, {rig.id("wh.transform.rotate"), "source"}).ok());
    REQUIRE(rig.player.step().ok());
    CHECK(rig.device.pipelineDescs().size() > pipelines);
    CHECK_FALSE(rig.player.status().lastError.has_value());
    CHECK(rig.player.status().running);
}

TEST_CASE("Player: Ausgang wechseln im Lauf baut die Session neu auf", "[player][edit]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    const std::size_t pipelines = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.setTarget({rig.id("wh.transform.mirror"), "out"}).ok());
    REQUIRE(rig.player.step().ok());
    CHECK(rig.device.pipelineDescs().size() > pipelines);
    CHECK(rig.player.status().running);
    CHECK_FALSE(rig.player.status().lastError.has_value());
}

TEST_CASE("Player: Bearbeiten, Speichern und Laden bewahrt Positionen und Graph", "[player][edit]") {
    Rig rig;
    const NodeId lfo = rig.id("wh.value.lfo");
    REQUIRE(rig.player.connect({lfo, "out"}, {rig.id("wh.transform.rotate"), "angle"}).ok());
    REQUIRE(rig.player.moveNode(lfo, Vec2{-40.5F, 99.0F}).ok());
    const auto added = rig.player.addNode("wh.generator.noise", Vec2{7.0F, 8.0F});
    REQUIRE(added.ok());

    const std::string path = (std::filesystem::temp_directory_path() / "whfx_player_edit_test.whfx").string();
    REQUIRE(rig.player.save(path).ok());
    CHECK_FALSE(rig.player.modified());

    runtime::Player other{&rig.device, *rig.library, rig.gate};
    REQUIRE(other.load(path).ok());
    std::filesystem::remove(path);
    const auto& g = other.project()->graph;
    CHECK(g.nodes().size() == 5);
    CHECK(g.connections().size() == 3);
    const auto it = std::ranges::find_if(g.nodes(), [](const auto& n) { return n.typeId == "wh.value.lfo"; });
    REQUIRE(it != g.nodes().end());
    CHECK(it->position == Vec2{-40.5F, 99.0F});
    const auto noise = std::ranges::find_if(g.nodes(), [](const auto& n) { return n.typeId == "wh.generator.noise"; });
    REQUIRE(noise != g.nodes().end());
    CHECK(noise->position == Vec2{7.0F, 8.0F});
}

TEST_CASE("Player: Bearbeiten ohne Projekt ist ein Fehler", "[player][edit]") {
    auto library = nodes::BuiltinLibrary::create().value();
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{nullptr, *library, gate};
    CHECK_FALSE(player.addNode("wh.generator.plasma", Vec2{}).ok());
    CHECK_FALSE(player.removeNode(NodeId{1}).ok());
    CHECK_FALSE(player.connect({NodeId{1}, "out"}, {NodeId{2}, "x"}).ok());
    CHECK_FALSE(player.disconnect(ConnectionId{1}).ok());
    CHECK_FALSE(player.moveNode(NodeId{1}, Vec2{}).ok());
    CHECK_FALSE(player.setTarget({NodeId{1}, "out"}).ok());
}

TEST_CASE("Player: Rückgängig und Wiederholen für Graphänderungen", "[player][undo]") {
    Rig rig;
    CHECK_FALSE(rig.player.canUndo());
    CHECK_FALSE(rig.player.canRedo());
    CHECK_FALSE(rig.player.undo().ok());
    CHECK_FALSE(rig.player.redo().ok());

    const std::size_t nodes = rig.player.project()->graph.nodes().size();
    const auto added = rig.player.addNode("wh.generator.noise", Vec2{});
    REQUIRE(added.ok());
    REQUIRE(rig.player.connect({added.value(), "out"}, {rig.id("wh.transform.mirror"), "source"}).ok());
    CHECK(rig.player.canUndo());

    REQUIRE(rig.player.undo().ok());  // Verbindung zurück: mirror.source hängt wieder an plasma
    const auto* c = rig.player.project()->graph.findInputConnection(rig.id("wh.transform.mirror"), "source");
    REQUIRE(c != nullptr);
    CHECK(c->from.node == rig.id("wh.generator.plasma"));
    CHECK(rig.player.canRedo());

    REQUIRE(rig.player.undo().ok());  // Node weg
    CHECK(rig.player.project()->graph.nodes().size() == nodes);
    CHECK_FALSE(rig.player.canUndo());

    REQUIRE(rig.player.redo().ok());
    REQUIRE(rig.player.redo().ok());
    CHECK(rig.player.project()->graph.nodes().size() == nodes + 1);
    CHECK(rig.player.project()->graph.findInputConnection(rig.id("wh.transform.mirror"), "source")->from.node ==
          rig.id("wh.generator.noise"));
    CHECK_FALSE(rig.player.canRedo());
}

TEST_CASE("Player: eine neue Änderung löscht die Wiederholen-Liste", "[player][undo]") {
    Rig rig;
    REQUIRE(rig.player.addNode("wh.generator.noise", Vec2{}).ok());
    REQUIRE(rig.player.undo().ok());
    REQUIRE(rig.player.canRedo());
    REQUIRE(rig.player.addNode("wh.generator.wave", Vec2{}).ok());
    CHECK_FALSE(rig.player.canRedo());
}

TEST_CASE("Player: abgelehnte Änderungen landen nicht in der Liste", "[player][undo]") {
    Rig rig;
    CHECK_FALSE(rig.player.addNode("gibt.es.nicht", Vec2{}).ok());
    CHECK_FALSE(rig.player.removeNode(NodeId{999}).ok());  // gibt es nicht
    CHECK_FALSE(rig.player.connect({rig.id("wh.transform.rotate"), "out"}, {rig.id("wh.transform.mirror"), "source"}).ok());
    CHECK_FALSE(rig.player.canUndo());
}

TEST_CASE("Player: Ziehen eines Reglers ist ein Schritt", "[player][undo]") {
    Rig rig;
    const NodeId lfo = rig.id("wh.value.lfo");
    for (int i = 1; i <= 30; ++i) {
        REQUIRE(rig.player.setParameter(lfo, "frequency", 0.1 * i).ok());
    }
    const double last = std::get<double>(rig.player.project()->graph.findNode(lfo)->parameters.at("frequency"));
    REQUIRE(rig.player.undo().ok());
    const double original = std::get<double>(rig.player.project()->graph.findNode(lfo)->parameters.at("frequency"));
    CHECK(original != last);
    CHECK_FALSE(rig.player.canUndo());  // alle 30 Änderungen waren ein Schritt

    REQUIRE(rig.player.redo().ok());
    CHECK(std::get<double>(rig.player.project()->graph.findNode(lfo)->parameters.at("frequency")) == last);

    // ein anderer Regler, dann derselbe wieder: zwei Schritte
    REQUIRE(rig.player.setParameter(lfo, "min", -10.0).ok());
    REQUIRE(rig.player.setParameter(lfo, "frequency", 5.0).ok());
    REQUIRE(rig.player.undo().ok());
    REQUIRE(rig.player.undo().ok());
    CHECK(std::get<double>(rig.player.project()->graph.findNode(lfo)->parameters.at("frequency")) == last);
}

TEST_CASE("Player: Verschieben ist ein Schritt je Node; Undo nach Undo setzt neu an", "[player][undo]") {
    Rig rig;
    const NodeId mirror = rig.id("wh.transform.mirror");
    const Vec2 start = rig.player.project()->graph.findNode(mirror)->position;
    for (int i = 1; i <= 10; ++i) {
        REQUIRE(rig.player.moveNode(mirror, Vec2{static_cast<float>(i), 0.0F}).ok());
    }
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->graph.findNode(mirror)->position == start);
    // nach Undo darf eine neue Verschiebung nicht mit der alten verschmelzen
    REQUIRE(rig.player.moveNode(mirror, Vec2{99.0F, 0.0F}).ok());
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->graph.findNode(mirror)->position == start);
}

TEST_CASE("Player: Undo im Lauf übernimmt den Graphen und den Ausgang", "[player][undo]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    const std::size_t pipelines = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.setTarget({rig.id("wh.transform.mirror"), "out"}).ok());
    REQUIRE(rig.player.step().ok());
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->target->node == rig.id("wh.transform.rotate"));
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.status().running);
    CHECK_FALSE(rig.player.status().lastError.has_value());
    CHECK(rig.device.pipelineDescs().size() > pipelines);
    CHECK(rig.player.modified());
}

TEST_CASE("Player: Laden leert die Listen, Speichern nicht", "[player][undo]") {
    Rig rig;
    REQUIRE(rig.player.addNode("wh.generator.noise", Vec2{}).ok());
    const std::string path = (std::filesystem::temp_directory_path() / "whfx_player_undo_test.whfx").string();
    REQUIRE(rig.player.save(path).ok());
    CHECK(rig.player.canUndo());
    REQUIRE(rig.player.loadText(kProject).ok());
    CHECK_FALSE(rig.player.canUndo());
    CHECK_FALSE(rig.player.canRedo());
    std::filesystem::remove(path);
}
