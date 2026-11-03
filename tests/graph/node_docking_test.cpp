// Tests von node_docking.hpp: Wohin ein aus der Palette gezogener Node einrastet (Kabel, Zielnode links/rechts).
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/node_docking.hpp"
#include "whitehole/graph/node_registry_impl.hpp"

using namespace whitehole;
using namespace whitehole::graph;

namespace {

PortDescriptor in(const char* key, PortType type) { return PortDescriptor{key, key, type, PortDirection::Input, false}; }
PortDescriptor out(const char* key, PortType type) { return PortDescriptor{key, key, type, PortDirection::Output, false}; }

NodeDescriptor describe(const char* id, std::vector<PortDescriptor> ports) {
    NodeDescriptor d;
    d.typeId = id;
    d.displayName = id;
    d.ports = std::move(ports);
    return d;
}

std::shared_ptr<NodeRegistry> makeRegistry() {
    auto registry = std::make_shared<NodeRegistry>();
    (void)registry->registerNode(describe("t.gen", {out("out", PortType::Field)}));
    (void)registry->registerNode(describe("t.fx", {in("src", PortType::Field), out("out", PortType::Field)}));
    (void)registry->registerNode(describe(
        "t.mix", {in("a", PortType::Field), in("b", PortType::Field), in("amount", PortType::Scalar), out("out", PortType::Field)}));
    (void)registry->registerNode(describe("t.lfo", {out("value", PortType::Scalar)}));
    (void)registry->registerNode(describe("t.sink", {in("src", PortType::Field)}));
    (void)registry->registerNode(describe("t.text", {in("tin", PortType::Text), out("tout", PortType::Text)}));
    (void)registry->registerNode(describe("t.audio", {out("spec", PortType::AudioSpectrum)}));
    return registry;
}

struct Fixture {
    std::shared_ptr<NodeRegistry> registry{makeRegistry()};
    Graph graph{registry};

    NodeId add(const char* type) { return graph.addNode(type, Vec2{}).value(); }
    const NodeDescriptor& type(const char* id) const { return *registry->find(id); }
};

}  // namespace

TEST_CASE("Docking auf ein Kabel fügt den Node dazwischen ein") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    const NodeId sink = f.add("t.sink");
    const ConnectionId wire = f.graph.connect({gen, "out"}, {sink, "src"}).value();

    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = wire;
    const auto plan = planDocking(f.graph, f.type("t.fx"), target);
    REQUIRE(plan.has_value());
    REQUIRE(plan->remove.size() == 1);
    CHECK(plan->remove[0] == wire);
    REQUIRE(plan->links.size() == 2);
    CHECK(plan->links[0].from == PortRef{gen, "out"});
    CHECK(plan->links[0].to == PortRef{NodeId{}, "src"});
    CHECK(plan->links[1].from == PortRef{NodeId{}, "out"});
    CHECK(plan->links[1].to == PortRef{sink, "src"});
}

TEST_CASE("Docking auf ein Kabel nimmt bei mehreren Eingängen den ersten des richtigen Typs") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    const NodeId sink = f.add("t.sink");
    const ConnectionId wire = f.graph.connect({gen, "out"}, {sink, "src"}).value();

    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = wire;
    const auto plan = planDocking(f.graph, f.type("t.mix"), target);
    REQUIRE(plan.has_value());
    REQUIRE(plan->links.size() == 2);
    CHECK(plan->links[0].to.portKey == "a");
}

TEST_CASE("Docking auf ein Kabel scheitert, wenn der Node nicht passt") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    const NodeId sink = f.add("t.sink");
    const ConnectionId wire = f.graph.connect({gen, "out"}, {sink, "src"}).value();

    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = wire;
    CHECK_FALSE(planDocking(f.graph, f.type("t.lfo"), target).has_value());   // kein Eingang
    CHECK_FALSE(planDocking(f.graph, f.type("t.text"), target).has_value());  // Text passt nicht zu Field
    target.wire = ConnectionId{99};
    CHECK_FALSE(planDocking(f.graph, f.type("t.fx"), target).has_value());    // unbekanntes Kabel
}

TEST_CASE("Linke Hälfte: der neue Node speist den ersten freien Eingang") {
    Fixture f;
    const NodeId sink = f.add("t.sink");

    DockTarget target;
    target.node = sink;
    const auto plan = planDocking(f.graph, f.type("t.gen"), target);
    REQUIRE(plan.has_value());
    CHECK(plan->remove.empty());
    REQUIRE(plan->links.size() == 1);
    CHECK(plan->links[0].from == PortRef{NodeId{}, "out"});
    CHECK(plan->links[0].to == PortRef{sink, "src"});
}

TEST_CASE("Linke Hälfte: bei mehreren Eingängen zählt der erste freie des richtigen Typs") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    const NodeId mix = f.add("t.mix");
    (void)f.graph.connect({gen, "out"}, {mix, "a"});

    DockTarget target;
    target.node = mix;
    const auto plan = planDocking(f.graph, f.type("t.gen"), target);
    REQUIRE(plan.has_value());
    REQUIRE(plan->links.size() == 1);
    CHECK(plan->links[0].to == PortRef{mix, "b"});  // a ist belegt, b frei
    CHECK(plan->remove.empty());
}

TEST_CASE("Linke Hälfte: ein Scalar-Node landet am Steuer-Eingang") {
    Fixture f;
    const NodeId mix = f.add("t.mix");
    DockTarget target;
    target.node = mix;
    const auto plan = planDocking(f.graph, f.type("t.lfo"), target);
    REQUIRE(plan.has_value());
    // Scalar passt auch auf Field-Eingänge (konstantes Feld); gleicher Typ geht vor.
    CHECK(plan->links[0].to == PortRef{mix, "amount"});
}

TEST_CASE("Die Mausposition wählt den Eingang") {
    Fixture f;
    const NodeId mix = f.add("t.mix");
    DockTarget target;
    target.node = mix;
    target.preferredPort = "b";
    const auto plan = planDocking(f.graph, f.type("t.gen"), target);
    REQUIRE(plan.has_value());
    CHECK(plan->links[0].to == PortRef{mix, "b"});

    target.preferredPort = "unbekannt";  // unpassend: automatisch
    const auto fallback = planDocking(f.graph, f.type("t.gen"), target);
    REQUIRE(fallback.has_value());
    CHECK(fallback->links[0].to == PortRef{mix, "a"});
}

TEST_CASE("Linke Hälfte: ein belegter Eingang bekommt den Node dazwischen") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    const NodeId sink = f.add("t.sink");
    const ConnectionId wire = f.graph.connect({gen, "out"}, {sink, "src"}).value();

    DockTarget target;
    target.node = sink;
    const auto plan = planDocking(f.graph, f.type("t.fx"), target);
    REQUIRE(plan.has_value());
    REQUIRE(plan->remove.size() == 1);
    CHECK(plan->remove[0] == wire);
    REQUIRE(plan->links.size() == 2);
    CHECK(plan->links[0].from == PortRef{gen, "out"});
    CHECK(plan->links[1].to == PortRef{sink, "src"});
}

TEST_CASE("Linke Hälfte: verträgt der neue Node den bisherigen Ausgang nicht, rastet nichts ein") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    const NodeId sink = f.add("t.sink");
    (void)f.graph.connect({gen, "out"}, {sink, "src"});

    DockTarget target;
    target.node = sink;
    // t.lfo hat einen Scalar-Ausgang (passt in src) und keinen Eingang: das Field-Kabel ginge verloren.
    CHECK_FALSE(planDocking(f.graph, f.type("t.lfo"), target).has_value());
}

TEST_CASE("Rechte Hälfte: der Ausgang des Zielnodes speist den neuen Node") {
    Fixture f;
    const NodeId gen = f.add("t.gen");
    DockTarget target;
    target.node = gen;
    target.rightHalf = true;
    const auto plan = planDocking(f.graph, f.type("t.fx"), target);
    REQUIRE(plan.has_value());
    CHECK(plan->remove.empty());
    REQUIRE(plan->links.size() == 1);
    CHECK(plan->links[0].from == PortRef{gen, "out"});
    CHECK(plan->links[0].to == PortRef{NodeId{}, "src"});
}

TEST_CASE("Rechte Hälfte: ohne passenden Ausgang oder Eingang rastet nichts ein") {
    Fixture f;
    const NodeId sink = f.add("t.sink");
    const NodeId audio = f.add("t.audio");
    DockTarget target;
    target.rightHalf = true;

    target.node = sink;  // keinen Ausgang
    CHECK_FALSE(planDocking(f.graph, f.type("t.fx"), target).has_value());
    target.node = audio;  // Spektrum passt in keinen Eingang von t.fx
    CHECK_FALSE(planDocking(f.graph, f.type("t.fx"), target).has_value());
    target.node = NodeId{77};  // unbekannt
    CHECK_FALSE(planDocking(f.graph, f.type("t.fx"), target).has_value());
}
