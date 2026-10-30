#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "test_nodes.hpp"
#include "whitehole/graph/graph_topology.hpp"

using namespace whitehole;
using namespace whitehole::graph;
using whitehole::test::makeTestRegistry;

namespace {

Connection edge(std::uint64_t id, std::uint64_t from, std::uint64_t to) {
    return Connection{ConnectionId{id}, PortRef{NodeId{from}, "out"}, PortRef{NodeId{to}, "in"}};
}

}  // namespace

TEST_CASE("isReachable folgt Verbindungen in Richtung der Datenflüsse") {
    const std::vector<Connection> chain{edge(1, 1, 2), edge(2, 2, 3)};

    CHECK(isReachable(chain, NodeId{1}, NodeId{3}));
    CHECK(isReachable(chain, NodeId{2}, NodeId{3}));
    CHECK_FALSE(isReachable(chain, NodeId{3}, NodeId{1}));
    CHECK_FALSE(isReachable(chain, NodeId{1}, NodeId{4}));
    CHECK(isReachable(chain, NodeId{2}, NodeId{2}));  // ein Node erreicht sich selbst
}

TEST_CASE("topologicalOrder: Vorgänger stehen vor Nachfolgern, Rest in Einfügereihenfolge") {
    Graph graph{makeTestRegistry()};
    // Absichtlich "verkehrt herum" angelegt: erst der Verbraucher, dann die Quellen.
    const NodeId filter = graph.addNode("test.filter", Vec2{}).value();
    const NodeId mixer = graph.addNode("test.mixer", Vec2{}).value();
    const NodeId first = graph.addNode("test.source", Vec2{}).value();
    const NodeId second = graph.addNode("test.source", Vec2{}).value();

    REQUIRE(graph.connect(PortRef{first, "out"}, PortRef{mixer, "a"}).ok());
    REQUIRE(graph.connect(PortRef{second, "out"}, PortRef{mixer, "b"}).ok());
    REQUIRE(graph.connect(PortRef{mixer, "out"}, PortRef{filter, "in"}).ok());

    const auto order = topologicalOrder(graph);
    REQUIRE(order.ok());

    const std::vector<NodeId> expected{first, second, mixer, filter};
    CHECK(order.value() == expected);
}

TEST_CASE("topologicalOrder: unverbundene Nodes bleiben in Einfügereihenfolge") {
    Graph graph{makeTestRegistry()};
    const NodeId a = graph.addNode("test.source", Vec2{}).value();
    const NodeId b = graph.addNode("test.label", Vec2{}).value();
    const NodeId c = graph.addNode("test.scalar", Vec2{}).value();

    const auto order = topologicalOrder(graph);
    REQUIRE(order.ok());
    const std::vector<NodeId> expected{a, b, c};
    CHECK(order.value() == expected);
}

TEST_CASE("topologicalOrder meldet Zyklen und unbekannte Nodes") {
    const std::vector<NodeId> nodes{NodeId{1}, NodeId{2}, NodeId{3}};

    const std::vector<Connection> cycle{edge(1, 1, 2), edge(2, 2, 3), edge(3, 3, 1)};
    const auto cyclic = topologicalOrder(nodes, cycle);
    REQUIRE_FALSE(cyclic.ok());
    CHECK(cyclic.error().code == ErrorCode::CycleDetected);

    const std::vector<Connection> dangling{edge(1, 1, 9)};
    const auto unknown = topologicalOrder(nodes, dangling);
    REQUIRE_FALSE(unknown.ok());
    CHECK(unknown.error().code == ErrorCode::NotFound);
}
