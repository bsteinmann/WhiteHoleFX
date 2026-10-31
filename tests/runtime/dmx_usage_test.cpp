// findDmxUsage / Player::dmxUsage: welche Universen die DMX-Nodes eines Graphen lesen.
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/dmx_usage.hpp"

using namespace whitehole;

TEST_CASE("findDmxUsage: Universen nach Protokoll getrennt", "[runtime][dmx]") {
    auto library = nodes::BuiltinLibrary::create();
    REQUIRE(library.ok());
    graph::Graph graph{library.value()->registry};

    CHECK_FALSE(runtime::findDmxUsage(graph).any());

    const NodeId art = graph.addNode("wh.value.dmx", Vec2{}).value();  // Art-Net, Universe 1 (Standard)
    const NodeId sacn = graph.addNode("wh.value.dmx", Vec2{}).value();
    REQUIRE(graph.setParameter(sacn, "source", graph::ParameterValue{std::int64_t{1}}).ok());
    REQUIRE(graph.setParameter(sacn, "universe", graph::ParameterValue{std::int64_t{12}}).ok());
    const NodeId color = graph.addNode("wh.value.dmxcolor", Vec2{}).value();
    REQUIRE(graph.setParameter(color, "source", graph::ParameterValue{std::int64_t{1}}).ok());
    REQUIRE(graph.setParameter(color, "universe", graph::ParameterValue{std::int64_t{3}}).ok());
    (void)graph.addNode("wh.value.lfo", Vec2{});  // andere Nodes zählen nicht
    (void)art;

    const runtime::DmxUsage usage = runtime::findDmxUsage(graph);
    CHECK(usage.any());
    CHECK(usage.artnet == std::set<std::uint32_t>{1});
    CHECK(usage.sacn == std::set<std::uint32_t>{3, 12});
}
