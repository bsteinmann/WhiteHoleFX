// Tests von node_placement.hpp: Wohin ein eingerasteter Node kommt und welche Nodes Platz machen.
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/node_docking.hpp"
#include "whitehole/graph/node_placement.hpp"
#include "whitehole/graph/node_registry_impl.hpp"

using namespace whitehole;
using namespace whitehole::graph;

namespace {

PortDescriptor in(const char* key) { return PortDescriptor{key, key, PortType::Field, PortDirection::Input, false}; }
PortDescriptor out(const char* key) { return PortDescriptor{key, key, PortType::Field, PortDirection::Output, false}; }

NodeDescriptor describe(const char* id, std::vector<PortDescriptor> ports) {
    NodeDescriptor d;
    d.typeId = id;
    d.displayName = id;
    d.ports = std::move(ports);
    return d;
}

struct Fixture {
    std::shared_ptr<NodeRegistry> registry = std::make_shared<NodeRegistry>();
    Graph graph{registry};
    std::vector<NodeRect> rects;

    Fixture() {
        (void)registry->registerNode(describe("t.gen", {out("out")}));
        (void)registry->registerNode(describe("t.fx", {in("src"), out("out")}));
        (void)registry->registerNode(describe("t.sink", {in("src")}));
    }
    NodeId add(const char* type, float x, float y) {
        const NodeId id = graph.addNode(type, Vec2{x, y}).value();
        rects.push_back({id, x, y, 190.0F, 60.0F});
        return id;
    }
    ConnectionId wire(NodeId from, NodeId to) { return graph.connect({from, "out"}, {to, "src"}).value(); }
    const NodeDescriptor& fx() const { return *registry->find("t.fx"); }
};

const Vec2 kSize{190.0F, 60.0F};
const Vec2 kMouse{5.0F, 7.0F};

bool moves(const Placement& p, NodeId id, float x) {
    for (const NodeMove& m : p.moved) {
        if (m.first == id) {
            return m.second.x == x;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("Kabel mit viel Platz: der Node sitzt mittig, niemand rückt") {
    Fixture f;
    const NodeId a = f.add("t.gen", 0, 0);
    const NodeId b = f.add("t.sink", 600, 100);
    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = f.wire(a, b);
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Placement p = placeDockedNode(f.graph, f.rects, *plan, target, kSize, kMouse);
    CHECK(p.position.x == 300.0F);  // 190 + (600 - 190 - 190) / 2
    CHECK(p.position.y == 50.0F);   // zwischen den Höhen der beiden
    CHECK(p.moved.empty());
}

TEST_CASE("Kabel ohne Platz: Ziel und was dahinter hängt rücken nach rechts, anderes bleibt") {
    Fixture f;
    const NodeId a = f.add("t.gen", 0, 0);
    const NodeId b = f.add("t.fx", 250, 0);
    const NodeId d = f.add("t.sink", 500, 0);
    const NodeId unrelated = f.add("t.gen", 250, 400);
    (void)f.wire(b, d);
    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = f.wire(a, b);
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Placement p = placeDockedNode(f.graph, f.rects, *plan, target, kSize, kMouse);
    CHECK(p.moved.size() == 2);
    CHECK(moves(p, b, 440.0F));  // Lücke 60, gebraucht 190 + 60: 190 Platz fehlen
    CHECK(moves(p, d, 690.0F));
    CHECK_FALSE(moves(p, unrelated, 250.0F));
    CHECK(p.position.x == 220.0F);
    CHECK(p.position.x >= 190.0F + 20.0F);
    CHECK(p.position.x + 190.0F + 20.0F <= 440.0F);
}

TEST_CASE("Kabel, das rückwärts läuft, bleibt bei der Mausposition") {
    Fixture f;
    const NodeId a = f.add("t.gen", 600, 0);
    const NodeId b = f.add("t.sink", 0, 0);
    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = f.wire(a, b);
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Placement p = placeDockedNode(f.graph, f.rects, *plan, target, kSize, kMouse);
    CHECK(p.position.x == kMouse.x);
    CHECK(p.moved.empty());
}

TEST_CASE("Links vor dem Ziel: neben dem Ziel, bei Bedarf rücken Ziel und Folgenodes") {
    Fixture f;
    const NodeId t = f.add("t.sink", 400, 0);
    DockTarget target;
    target.node = t;
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Placement free = placeDockedNode(f.graph, f.rects, *plan, target, kSize, kMouse);
    CHECK(free.position.x == 180.0F);  // 400 - 190 - 30
    CHECK(free.position.y == 0.0F);
    CHECK(free.moved.empty());

    // Davor steht schon ein Node: das Ziel rückt weg
    Fixture g;
    const NodeId a = g.add("t.gen", 100, 0);
    const NodeId t2 = g.add("t.sink", 400, 0);
    (void)g.wire(a, t2);
    DockTarget target2;
    target2.node = t2;
    const auto plan2 = planDocking(g.graph, g.fx(), target2);
    REQUIRE(plan2.has_value());
    const Placement tight = placeDockedNode(g.graph, g.rects, *plan2, target2, kSize, kMouse);
    REQUIRE(tight.moved.size() == 1);
    CHECK(tight.moved[0].first == t2);
    CHECK(tight.position.x >= 100.0F + 190.0F + 20.0F);                    // hinter dem Vorgänger
    CHECK(tight.position.x + 190.0F + 30.0F == tight.moved[0].second.x);   // direkt vor dem Ziel
}

TEST_CASE("Rechts hinter dem Ziel: was dahinter hängt, rückt nach rechts") {
    Fixture f;
    const NodeId t = f.add("t.gen", 0, 0);
    const NodeId d = f.add("t.sink", 210, 0);
    (void)f.wire(t, d);
    DockTarget target;
    target.node = t;
    target.rightHalf = true;
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Placement p = placeDockedNode(f.graph, f.rects, *plan, target, kSize, kMouse);
    CHECK(p.position.x == 220.0F);  // 190 + 30
    REQUIRE(p.moved.size() == 1);
    CHECK(moves(p, d, 440.0F));     // 410 + 30
}

TEST_CASE("Ein fremder Node im Weg: der neue Node weicht nach unten aus") {
    Fixture f;
    const NodeId t = f.add("t.gen", 0, 0);
    (void)f.add("t.gen", 220, 0);  // sitzt genau dort, wo der Node hinkäme, und hängt nicht an t
    DockTarget target;
    target.node = t;
    target.rightHalf = true;
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Placement p = placeDockedNode(f.graph, f.rects, *plan, target, kSize, kMouse);
    CHECK(p.moved.empty());
    CHECK(p.position.y >= 60.0F + 20.0F);
}

TEST_CASE("Unbekannte Nodes: die Mausposition bleibt") {
    Fixture f;
    DockTarget target;
    target.node = NodeId{99};
    DockPlan plan;
    const Placement p = placeDockedNode(f.graph, f.rects, plan, target, kSize, kMouse);
    CHECK(p.position.x == kMouse.x);
    CHECK(p.position.y == kMouse.y);
    CHECK(p.moved.empty());
}

TEST_CASE("Rückwärts laufendes Kabel mit Maus auf einem Node: der neue Node weicht aus") {
    Fixture f;
    const NodeId a = f.add("t.gen", 600, 0);
    const NodeId b = f.add("t.sink", 0, 0);
    DockTarget target;
    target.kind = DockTarget::Kind::Wire;
    target.wire = f.wire(a, b);
    const auto plan = planDocking(f.graph, f.fx(), target);
    REQUIRE(plan.has_value());
    const Vec2 onNode{f.rects[1].x, f.rects[1].y};
    const Placement p = placeDockedNode(f.graph, f.rects, *plan, target, kSize, onNode);
    for (const NodeRect& r : f.rects) {
        const bool apart = p.position.x + kSize.x <= r.x || r.x + r.w <= p.position.x || p.position.y + kSize.y <= r.y ||
                           r.y + r.h <= p.position.y;
        CHECK(apart);
    }
}
