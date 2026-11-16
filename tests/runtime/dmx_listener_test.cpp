// findDmxListenerInGraph: welcher Node einen bestimmten empfangenen DMX-Kanal liest.
// findDmxValueNodes: alle wh.value.dmx/wh.value.dmxcolor-Nodes des Graphen (für die Rechtsklick-Kategorie
// "Node" im DMX-Monitor, Eingang).
// findDmxOutputValueNodes: dasselbe für wh.output.dmx/wh.output.dmxcolor (Rechtsklick-Kategorie "Node" im
// DMX-Ausgangs-Monitor), inklusive der berechneten Pool-Kanal-Zahl je Typ/Parameter.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/dmx_listener.hpp"

using namespace whitehole;

TEST_CASE("findDmxListenerInGraph: DMX Channel und DMX Color, 16 Bit/Dimmer-Breite berücksichtigt", "[runtime][dmx]") {
    auto library = nodes::BuiltinLibrary::create();
    REQUIRE(library.ok());
    graph::Graph graph{library.value()->registry};

    const NodeId channel = graph.addNode("wh.value.dmx", Vec2{}).value();  // Art-Net, Universe 1, Kanal 1 (Standard)
    REQUIRE(graph.setParameter(channel, "bits16", graph::ParameterValue{true}).ok());  // belegt Kanal 1+2

    const NodeId color = graph.addNode("wh.value.dmxcolor", Vec2{}).value();
    REQUIRE(graph.setParameter(color, "universe", graph::ParameterValue{std::int64_t{5}}).ok());
    REQUIRE(graph.setParameter(color, "channel", graph::ParameterValue{std::int64_t{10}}).ok());
    REQUIRE(graph.setParameter(color, "mode", graph::ParameterValue{std::int64_t{1}}).ok());  // RGB+Dimmer: 4 Kanäle

    // Kanal 1 und 2 (16 Bit) gehören zum Channel-Node, Kanal 3 nicht mehr.
    auto hit1 = runtime::findDmxListenerInGraph(graph, input::DmxProtocol::ArtNet, 1, 1);
    REQUIRE(hit1.has_value());
    CHECK(hit1->name == "DMX Channel");
    CHECK(hit1->count == 2);
    CHECK(hit1->kind == runtime::DmxInputListener::Kind::Node);
    auto hit2 = runtime::findDmxListenerInGraph(graph, input::DmxProtocol::ArtNet, 1, 2);
    CHECK(hit2.has_value());
    CHECK_FALSE(runtime::findDmxListenerInGraph(graph, input::DmxProtocol::ArtNet, 1, 3).has_value());

    // Kanal 10..13 (4 Kanäle, RGB+Dimmer) gehören zum Color-Node auf Universe 5.
    auto colorHit = runtime::findDmxListenerInGraph(graph, input::DmxProtocol::ArtNet, 5, 13);
    REQUIRE(colorHit.has_value());
    CHECK(colorHit->name == "DMX Color");
    CHECK_FALSE(runtime::findDmxListenerInGraph(graph, input::DmxProtocol::ArtNet, 5, 14).has_value());
    // Anderes Protokoll (sACN) betrifft denselben Kanal nicht.
    CHECK_FALSE(runtime::findDmxListenerInGraph(graph, input::DmxProtocol::Sacn, 5, 10).has_value());
}

TEST_CASE("findDmxValueNodes: listet alle DMX-Wert-Nodes, andere Node-Typen nicht", "[runtime][dmx]") {
    auto library = nodes::BuiltinLibrary::create();
    REQUIRE(library.ok());
    graph::Graph graph{library.value()->registry};

    CHECK(runtime::findDmxValueNodes(graph).empty());

    const NodeId channel = graph.addNode("wh.value.dmx", Vec2{}).value();
    const NodeId color = graph.addNode("wh.value.dmxcolor", Vec2{}).value();
    const NodeId other = graph.addNode("wh.generator.gradient", Vec2{}).value();
    (void)other;

    const auto nodes = runtime::findDmxValueNodes(graph);
    REQUIRE(nodes.size() == 2);
    CHECK(nodes[0].id == channel);
    CHECK(nodes[0].label == "DMX Channel");
    CHECK(nodes[1].id == color);
    CHECK(nodes[1].label == "DMX Color");
}

TEST_CASE("findDmxOutputValueNodes: listet DMX-Ausgabe-Nodes mit ihrem Pool-Kanal/Zahl, andere Typen nicht",
          "[runtime][dmx]") {
    auto library = nodes::BuiltinLibrary::create();
    REQUIRE(library.ok());
    graph::Graph graph{library.value()->registry};

    CHECK(runtime::findDmxOutputValueNodes(graph).empty());

    // Standard: channel 1, count 8, kein 16 Bit -> 8 Pool-Kanäle ab 1.
    const NodeId dmxOut = graph.addNode("wh.output.dmx", Vec2{}).value();
    // channel 10, count 3, 16 Bit -> 6 Pool-Kanäle (3 * 2) ab 10.
    const NodeId wideOut = graph.addNode("wh.output.dmx", Vec2{}).value();
    REQUIRE(graph.setParameter(wideOut, "channel", graph::ParameterValue{std::int64_t{10}}).ok());
    REQUIRE(graph.setParameter(wideOut, "count", graph::ParameterValue{std::int64_t{3}}).ok());
    REQUIRE(graph.setParameter(wideOut, "bits16", graph::ParameterValue{true}).ok());
    // channel 20, mode 3 (Dimmer+RGBW) -> 5 Pool-Kanäle ab 20.
    const NodeId colorOut = graph.addNode("wh.output.dmxcolor", Vec2{}).value();
    REQUIRE(graph.setParameter(colorOut, "channel", graph::ParameterValue{std::int64_t{20}}).ok());
    REQUIRE(graph.setParameter(colorOut, "mode", graph::ParameterValue{std::int64_t{3}}).ok());
    const NodeId other = graph.addNode("wh.generator.gradient", Vec2{}).value();
    (void)other;

    const auto refs = runtime::findDmxOutputValueNodes(graph);
    REQUIRE(refs.size() == 3);
    CHECK(refs[0].id == dmxOut);
    // Zwei wh.output.dmx-Nodes ohne eigenen Namen: durchnummeriert, damit sie im "Node"-Menü des
    // DMX-Monitors unterscheidbar sind (sonst stünden beide als "DMX Out" da, siehe node_display_name.hpp).
    CHECK(refs[0].label == "DMX Out 1");
    CHECK(refs[0].poolChannel == 1);
    CHECK(refs[0].count == 8);
    CHECK(refs[1].id == wideOut);
    CHECK(refs[1].label == "DMX Out 2");
    CHECK(refs[1].poolChannel == 10);
    CHECK(refs[1].count == 6);
    CHECK(refs[2].id == colorOut);
    CHECK(refs[2].label == "DMX Color Out");
    CHECK(refs[2].poolChannel == 20);
    CHECK(refs[2].count == 5);
    CHECK_FALSE(refs[0].lookIndex.has_value());  // direkt im Graphen, nicht über Player::dmxOutputValueNodes()
}
