// Gruppen im Graphen: anlegen, prüfen, ändern, auflösen, und was beim Entfernen von Nodes geschieht.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "test_nodes.hpp"
#include "whitehole/graph/graph_model.hpp"

using namespace whitehole;
using namespace whitehole::graph;
using whitehole::test::makeTestRegistry;

namespace {

struct Rig {
    Graph graph{makeTestRegistry()};
    NodeId source = graph.addNode("test.source", Vec2{}).value();
    NodeId filter = graph.addNode("test.filter", Vec2{}).value();
    NodeId mixer = graph.addNode("test.mixer", Vec2{}).value();

    [[nodiscard]] NodeGroup group() const {
        NodeGroup g;
        g.name = "Plasma";
        g.members = {source, filter};
        g.ports = {{filter, "in", "Eingang"}, {filter, "out", "Ausgang"}};
        g.parameters = {{source, "gain", "Stärke"}};
        return g;
    }
};

}  // namespace

TEST_CASE("Gruppe: anlegen und nachschlagen", "[graph][group]") {
    Rig rig;
    REQUIRE(rig.graph.addGroup(rig.group()).ok());
    const NodeGroup* group = rig.graph.findGroup("Plasma");
    REQUIRE(group != nullptr);
    CHECK(group->members.size() == 2);
    CHECK(group->collapsed);
    CHECK(rig.graph.groupOf(rig.source) == group);
    CHECK(rig.graph.groupOf(rig.filter) == group);
    CHECK(rig.graph.groupOf(rig.mixer) == nullptr);
    CHECK(rig.graph.findGroup("gibt es nicht") == nullptr);
}

TEST_CASE("Gruppe: ungültige Gruppen werden abgelehnt", "[graph][group]") {
    Rig rig;
    NodeGroup g = rig.group();

    g.name.clear();
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);
    g = rig.group();
    g.name = std::string(kMaxGroupNameLength + 1, 'x');
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    g = rig.group();
    g.members.clear();
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    g = rig.group();
    g.members.push_back(NodeId{999});
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::NotFound);

    g = rig.group();
    g.members.push_back(rig.source);  // doppelt
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    g = rig.group();
    g.ports.push_back({rig.mixer, "a", "Fremd"});  // kein Mitglied
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    g = rig.group();
    g.ports.push_back({rig.filter, "gibtsnicht", "X"});
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::NotFound);

    g = rig.group();
    g.ports.push_back({rig.filter, "in", "Doppelt"});
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    g = rig.group();
    g.ports[0].label.clear();
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    g = rig.group();
    g.parameters.push_back({rig.source, "gibtsnicht", "X"});
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::NotFound);

    g = rig.group();
    g.position.x = std::numeric_limits<float>::infinity();
    CHECK(rig.graph.addGroup(g).error().code == ErrorCode::InvalidArgument);

    CHECK(rig.graph.groups().empty());
}

TEST_CASE("Gruppe: Name einmalig, ein Node nur in einer Gruppe", "[graph][group]") {
    Rig rig;
    REQUIRE(rig.graph.addGroup(rig.group()).ok());
    CHECK(rig.graph.addGroup(rig.group()).error().code == ErrorCode::AlreadyExists);  // Name

    NodeGroup other;
    other.name = "Zweite";
    other.members = {rig.filter};  // steckt schon in "Plasma"
    CHECK(rig.graph.addGroup(other).error().code == ErrorCode::AlreadyExists);
    other.members = {rig.mixer};
    CHECK(rig.graph.addGroup(other).ok());
}

TEST_CASE("Gruppe: ersetzen, zuklappen, verschieben, auflösen", "[graph][group]") {
    Rig rig;
    REQUIRE(rig.graph.addGroup(rig.group()).ok());

    NodeGroup changed = rig.group();
    changed.name = "Plasma neu";  // Umbenennen ersetzt sich selbst, ohne Namenskonflikt
    changed.ports.pop_back();
    REQUIRE(rig.graph.replaceGroup("Plasma", changed).ok());
    CHECK(rig.graph.findGroup("Plasma") == nullptr);
    REQUIRE(rig.graph.findGroup("Plasma neu") != nullptr);
    CHECK(rig.graph.findGroup("Plasma neu")->ports.size() == 1);

    NodeGroup bad = changed;
    bad.members.clear();
    CHECK_FALSE(rig.graph.replaceGroup("Plasma neu", bad).ok());
    CHECK(rig.graph.findGroup("Plasma neu")->members.size() == 2);  // bleibt, wie sie war
    CHECK(rig.graph.replaceGroup("nix", changed).error().code == ErrorCode::NotFound);

    REQUIRE(rig.graph.setGroupCollapsed("Plasma neu", false).ok());
    CHECK_FALSE(rig.graph.findGroup("Plasma neu")->collapsed);
    REQUIRE(rig.graph.setGroupPosition("Plasma neu", Vec2{10.0F, 20.0F}).ok());
    CHECK(rig.graph.findGroup("Plasma neu")->position.x == 10.0F);
    CHECK_FALSE(rig.graph.setGroupPosition("Plasma neu", Vec2{std::nanf(""), 0.0F}).ok());
    CHECK(rig.graph.setGroupCollapsed("nix", true).error().code == ErrorCode::NotFound);

    REQUIRE(rig.graph.removeGroup("Plasma neu").ok());
    CHECK(rig.graph.groups().empty());
    CHECK(rig.graph.findNode(rig.source) != nullptr);  // die Nodes bleiben
    CHECK(rig.graph.removeGroup("Plasma neu").error().code == ErrorCode::NotFound);
}

TEST_CASE("Gruppe: ein entfernter Node verlässt die Gruppe und ihre Schnittstelle", "[graph][group]") {
    Rig rig;
    REQUIRE(rig.graph.addGroup(rig.group()).ok());
    REQUIRE(rig.graph.removeNode(rig.filter).ok());
    const NodeGroup* group = rig.graph.findGroup("Plasma");
    REQUIRE(group != nullptr);
    CHECK(group->members.size() == 1);
    CHECK(group->ports.empty());               // beide Ports gehörten zum Filter
    CHECK(group->parameters.size() == 1);      // der Regler gehört zur Quelle
    REQUIRE(rig.graph.removeNode(rig.source).ok());
    CHECK(rig.graph.groups().empty());         // keine Mitglieder mehr: die Gruppe verschwindet
}

TEST_CASE("Gruppe: eine Kopie des Graphen nimmt die Gruppen mit", "[graph][group]") {
    Rig rig;
    REQUIRE(rig.graph.addGroup(rig.group()).ok());
    const Graph copy = rig.graph;
    CHECK(copy.groups().size() == 1);
}
