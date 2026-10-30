#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "test_nodes.hpp"
#include "whitehole/graph/graph_model.hpp"

using namespace whitehole;
using namespace whitehole::graph;
using whitehole::test::makeTestRegistry;

namespace {

NodeId add(Graph& graph, std::string_view typeId) {
    return graph.addNode(typeId, Vec2{}).value();
}

PortRef out(NodeId node) { return PortRef{node, "out"}; }

}  // namespace

TEST_CASE("Graph::addNode: unbekannter Typ, Standardwerte, aufsteigende IDs") {
    Graph graph{makeTestRegistry()};

    const auto missing = graph.addNode("does.not.exist", Vec2{});
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);

    const NodeId first = add(graph, "test.source");
    const NodeId second = add(graph, "test.source");
    CHECK(first == NodeId{1});
    CHECK(second == NodeId{2});

    const NodeInstance* node = graph.findNode(first);
    REQUIRE(node != nullptr);
    CHECK(node->typeId == "test.source");
    CHECK(node->label == "Source");
    CHECK(std::get<double>(node->parameters.at("gain")) == 0.5);
    CHECK(std::get<std::int64_t>(node->parameters.at("count")) == 1);
    CHECK(std::get<std::string>(node->parameters.at("name")) == "x");
}

TEST_CASE("Graph::connect: Erfolg, Auffinden und erlaubte Konvertierung") {
    Graph graph{makeTestRegistry()};
    const NodeId source = add(graph, "test.source");
    const NodeId filter = add(graph, "test.filter");

    const auto link = graph.connect(out(source), PortRef{filter, "in"});
    REQUIRE(link.ok());
    CHECK(graph.connections().size() == 1U);

    const Connection* found = graph.findConnection(link.value());
    REQUIRE(found != nullptr);
    CHECK(found->from == out(source));

    const Connection* byInput = graph.findInputConnection(filter, "in");
    REQUIRE(byInput != nullptr);
    CHECK(byInput->id == link.value());
    CHECK(graph.findInputConnection(filter, "nope") == nullptr);

    // Scalar -> Field ist als konstantes Feld erlaubt.
    const NodeId second = add(graph, "test.filter");
    const NodeId scalar = add(graph, "test.scalar");
    CHECK(graph.connect(out(scalar), PortRef{second, "in"}).ok());
}

TEST_CASE("Graph::connect: ungültige Verbindungen werden mit dem richtigen Fehler abgelehnt") {
    Graph graph{makeTestRegistry()};
    const NodeId source = add(graph, "test.source");
    const NodeId filter = add(graph, "test.filter");
    const NodeId other = add(graph, "test.filter");
    const NodeId label = add(graph, "test.label");

    const auto noNode = graph.connect(PortRef{NodeId{99}, "out"}, PortRef{filter, "in"});
    REQUIRE_FALSE(noNode.ok());
    CHECK(noNode.error().code == ErrorCode::NotFound);

    const auto noPort = graph.connect(PortRef{source, "nope"}, PortRef{filter, "in"});
    REQUIRE_FALSE(noPort.ok());
    CHECK(noPort.error().code == ErrorCode::NotFound);

    const auto wrongDirection = graph.connect(PortRef{filter, "in"}, PortRef{other, "out"});
    REQUIRE_FALSE(wrongDirection.ok());
    CHECK(wrongDirection.error().code == ErrorCode::IncompatiblePorts);

    const auto wrongType = graph.connect(out(label), PortRef{filter, "in"});
    REQUIRE_FALSE(wrongType.ok());
    CHECK(wrongType.error().code == ErrorCode::IncompatiblePorts);

    CHECK(graph.connections().empty());
}

TEST_CASE("Graph::connect: Zyklen werden erkannt, auch die Selbstverbindung") {
    Graph graph{makeTestRegistry()};
    const NodeId a = add(graph, "test.filter");
    const NodeId b = add(graph, "test.filter");
    const NodeId c = add(graph, "test.filter");

    REQUIRE(graph.connect(out(a), PortRef{b, "in"}).ok());
    REQUIRE(graph.connect(out(b), PortRef{c, "in"}).ok());

    const auto closing = graph.connect(out(c), PortRef{a, "in"});
    REQUIRE_FALSE(closing.ok());
    CHECK(closing.error().code == ErrorCode::CycleDetected);

    const auto self = graph.connect(out(a), PortRef{a, "in"});
    REQUIRE_FALSE(self.ok());
    CHECK(self.error().code == ErrorCode::CycleDetected);

    CHECK(graph.connections().size() == 2U);
}

TEST_CASE("Graph::connect: ein belegter Eingang wird nicht stillschweigend ersetzt") {
    Graph graph{makeTestRegistry()};
    const NodeId first = add(graph, "test.source");
    const NodeId second = add(graph, "test.source");
    const NodeId filter = add(graph, "test.filter");

    const auto link = graph.connect(out(first), PortRef{filter, "in"});
    REQUIRE(link.ok());

    const auto occupied = graph.connect(out(second), PortRef{filter, "in"});
    REQUIRE_FALSE(occupied.ok());
    CHECK(occupied.error().code == ErrorCode::AlreadyExists);

    REQUIRE(graph.disconnect(link.value()).ok());
    CHECK(graph.connect(out(second), PortRef{filter, "in"}).ok());

    const auto gone = graph.disconnect(link.value());
    REQUIRE_FALSE(gone.ok());
    CHECK(gone.error().code == ErrorCode::NotFound);
}

TEST_CASE("Graph::removeNode entfernt Verbindungen, IDs werden nicht wiederverwendet") {
    Graph graph{makeTestRegistry()};
    const NodeId source = add(graph, "test.source");
    const NodeId filter = add(graph, "test.filter");
    REQUIRE(graph.connect(out(source), PortRef{filter, "in"}).ok());

    REQUIRE(graph.removeNode(source).ok());
    CHECK(graph.connections().empty());
    CHECK(graph.findNode(source) == nullptr);
    CHECK(graph.findNode(filter) != nullptr);

    const auto again = graph.removeNode(source);
    REQUIRE_FALSE(again.ok());
    CHECK(again.error().code == ErrorCode::NotFound);

    CHECK(add(graph, "test.source") == NodeId{3});
}

TEST_CASE("Graph::setParameter: Typprüfung, Begrenzung, nicht endliche Werte") {
    Graph graph{makeTestRegistry()};
    const NodeId source = add(graph, "test.source");
    const auto gain = [&] { return std::get<double>(graph.findNode(source)->parameters.at("gain")); };
    const auto count = [&] {
        return std::get<std::int64_t>(graph.findNode(source)->parameters.at("count"));
    };

    REQUIRE(graph.setParameter(source, "gain", 0.25).ok());
    CHECK(gain() == 0.25);

    // Begrenzung auf [0, 1] beziehungsweise [0, 10]
    REQUIRE(graph.setParameter(source, "gain", 7.0).ok());
    CHECK(gain() == 1.0);
    REQUIRE(graph.setParameter(source, "count", std::int64_t{99}).ok());
    CHECK(count() == 10);
    REQUIRE(graph.setParameter(source, "count", std::int64_t{-5}).ok());
    CHECK(count() == 0);

    // Typ muss zum Standardwert passen
    const auto wrongType = graph.setParameter(source, "gain", std::int64_t{1});
    REQUIRE_FALSE(wrongType.ok());
    CHECK(wrongType.error().code == ErrorCode::InvalidArgument);

    // NaN und Unendlich werden abgelehnt, der alte Wert bleibt
    REQUIRE(graph.setParameter(source, "gain", 0.5).ok());
    const auto nan = graph.setParameter(source, "gain", std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(nan.ok());
    CHECK(nan.error().code == ErrorCode::InvalidArgument);
    CHECK_FALSE(graph.setParameter(source, "gain", std::numeric_limits<double>::infinity()).ok());
    CHECK_FALSE(graph.setParameter(source, "tint",
                                   ColorRgba{std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F, 1.0F})
                    .ok());
    CHECK(gain() == 0.5);

    // Nicht begrenzte Typen
    CHECK(graph.setParameter(source, "name", std::string{"hello"}).ok());
    CHECK(graph.setParameter(source, "tint", ColorRgba{0.1F, 0.2F, 0.3F, 1.0F}).ok());

    const auto unknown = graph.setParameter(source, "nope", 1.0);
    REQUIRE_FALSE(unknown.ok());
    CHECK(unknown.error().code == ErrorCode::NotFound);
    CHECK(graph.setParameter(NodeId{42}, "gain", 1.0).error().code == ErrorCode::NotFound);
}

TEST_CASE("Graph: eine Kopie ist ein unabhängiger Snapshot") {
    Graph graph{makeTestRegistry()};
    const NodeId source = add(graph, "test.source");

    const Graph snapshot = graph;
    add(graph, "test.filter");
    REQUIRE(graph.setParameter(source, "gain", 0.9).ok());

    CHECK(graph.nodes().size() == 2U);
    CHECK(snapshot.nodes().size() == 1U);
    CHECK(std::get<double>(snapshot.findNode(source)->parameters.at("gain")) == 0.5);
}

TEST_CASE("Graph: setPosition verschiebt den Node und prüft die Eingabe") {
    Graph g{makeTestRegistry()};
    const NodeId id = add(g, "test.source");
    CHECK(g.setPosition(id, Vec2{12.0F, -3.5F}).ok());
    CHECK(g.findNode(id)->position == Vec2{12.0F, -3.5F});
    CHECK(g.setPosition(id, Vec2{std::numeric_limits<float>::quiet_NaN(), 0.0F}).error().code ==
          ErrorCode::InvalidArgument);
    CHECK(g.findNode(id)->position == Vec2{12.0F, -3.5F});
    CHECK(g.setPosition(NodeId{999}, Vec2{}).error().code == ErrorCode::NotFound);
}
