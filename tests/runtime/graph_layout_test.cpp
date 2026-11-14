// Tests der automatischen Node-Anordnung.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/graph_layout.hpp"

using namespace whitehole;

namespace {

struct Chain {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    graph::Graph graph{library->registry};
    NodeId plasma, mirror, scale, lfo;

    Chain() {
        plasma = graph.addNode("wh.generator.plasma", Vec2{}).value();
        mirror = graph.addNode("wh.transform.mirror", Vec2{}).value();
        scale = graph.addNode("wh.transform.scale", Vec2{}).value();
        lfo = graph.addNode("wh.value.lfo", Vec2{}).value();
        REQUIRE(graph.connect({plasma, "out"}, {mirror, "source"}).ok());
        REQUIRE(graph.connect({mirror, "out"}, {scale, "source"}).ok());
        REQUIRE(graph.connect({lfo, "out"}, {scale, "factor"}).ok());
    }
};

}  // namespace

TEST_CASE("Layout: Quellen links, Verbraucher rechts, gleiche Spalte untereinander", "[layout]") {
    Chain c;
    const auto positions = runtime::layerLayout(c.graph);
    REQUIRE(positions.size() == 4);
    CHECK(positions[0].x == 0.0F);                 // plasma: Quelle
    CHECK(positions[1].x == 260.0F);               // mirror
    CHECK(positions[2].x == 520.0F);               // scale
    CHECK(positions[3].x == 0.0F);                 // lfo: Quelle
    CHECK(positions[0].y == 0.0F);
    // unter plasma, mit genug Abstand fuer dessen tatsaechliche Kartenhoehe (8 Parameter -> 8 Portzeilen):
    // 26 (Titel) + 12 + 8 * 22 (Zeilen) + 24 (Luecke) = 238. Ein fester Wert haette plasma (oder jeden anderen,
    // spaeter gewachsenen Quell-Node) ueberlappt - siehe die eigene Ueberlappungs-Pruefung unten.
    CHECK(positions[3].y == 238.0F);
}

TEST_CASE("Layout: Nodes derselben Spalte überlappen sich nie, auch bei vielen Parametern", "[layout]") {
    // Regression: Plasma wuchs 0.92.0 von 3 auf 8 Parameter (Farbpaletten); eine feste Zeilenhoehe liess die
    // Karte dann in ihren Nachbarn (hier: lfo, beide Spalte 0) hineinragen. Prueft das allgemein und nicht nur
    // fuer Plasmas aktuelle Parameterzahl, falls weitere Nodes spaeter aehnlich wachsen.
    Chain c;
    const auto positions = runtime::layerLayout(c.graph);
    const graph::NodeDescriptor* plasmaType = c.graph.registry().find("wh.generator.plasma");
    REQUIRE(plasmaType != nullptr);
    int plasmaPorts = 0;
    for (const auto& port : plasmaType->ports) {
        if (port.direction == graph::PortDirection::Input) {
            ++plasmaPorts;
        }
    }
    const float plasmaHeight = 26.0F + 12.0F + static_cast<float>(std::max(1, plasmaPorts)) * 22.0F;
    CHECK(positions[3].y >= positions[0].y + plasmaHeight);  // lfo beginnt erst, wenn plasma fertig ist
}

TEST_CASE("Layout: nur wenn alle Positionen fehlen", "[layout]") {
    Chain c;
    CHECK(runtime::needsLayout(c.graph));
    CHECK(runtime::applyLayoutIfMissing(c.graph));
    CHECK(c.graph.findNode(c.scale)->position.x == 520.0F);
    CHECK_FALSE(runtime::needsLayout(c.graph));
    CHECK_FALSE(runtime::applyLayoutIfMissing(c.graph));

    graph::Graph single{c.library->registry};
    (void)single.addNode("wh.generator.plasma", Vec2{});
    CHECK_FALSE(runtime::needsLayout(single));  // ein einzelner Node liegt zu Recht auf 0 0
}

TEST_CASE("Layout: gesetzte Positionen bleiben unangetastet", "[layout]") {
    Chain c;
    REQUIRE(c.graph.setPosition(c.mirror, Vec2{5.0F, 7.0F}).ok());
    CHECK_FALSE(runtime::applyLayoutIfMissing(c.graph));
    CHECK(c.graph.findNode(c.mirror)->position == Vec2{5.0F, 7.0F});
}
