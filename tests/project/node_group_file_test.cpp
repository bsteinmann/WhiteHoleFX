// Gruppen von Nodes in der Projektdatei: Lesen, Schreiben, Fehlermeldungen, auch im look-Block.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using project::parseProject;
using project::writeProject;

namespace {

struct Library {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    Library() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }
};

const std::string kNodes = R"(whitehole-project 1

node n1 wh.generator.shapes
  at 0 0
end
node n2 wh.transform.tile
  at 200 0
end
node n3 wh.output.canvas
  at 400 0
end
connect n1.out -> n2.source
connect n2.out -> n3.source
)";

const std::string kGroup = R"(nodegroup "Kacheln \"neu\""
  at 120 40
  color 0.5 0.25 1
  expanded
  member n1 n2
  port n2.out as "Ausgang"
  port n2.source as "Eingang"
  param n1.speed as "Tempo"
end
output n3.out
)";

}  // namespace

TEST_CASE("Gruppe in der Datei: lesen", "[project][group]") {
    const Library lib;
    const auto parsed = parseProject(kNodes + kGroup, lib.registry);
    REQUIRE(parsed.ok());
    const auto& groups = parsed.value().graph.groups();
    REQUIRE(groups.size() == 1);
    const graph::NodeGroup& group = groups.front();
    CHECK(group.name == "Kacheln \"neu\"");
    CHECK(group.position.x == 120.0F);
    CHECK(group.color.y == 0.25F);
    CHECK_FALSE(group.collapsed);
    CHECK(group.members.size() == 2);
    REQUIRE(group.ports.size() == 2);
    CHECK(group.ports[0].portKey == "out");
    CHECK(group.ports[0].label == "Ausgang");
    REQUIRE(group.parameters.size() == 1);
    CHECK(group.parameters[0].key == "speed");
}

TEST_CASE("Gruppe in der Datei: Schreiben und wieder Lesen ergibt denselben Text", "[project][group]") {
    const Library lib;
    const auto first = parseProject(kNodes + kGroup, lib.registry);
    REQUIRE(first.ok());
    const auto text = writeProject(first.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("nodegroup") != std::string::npos);
    const auto second = parseProject(text.value(), lib.registry);
    REQUIRE(second.ok());
    CHECK(second.value().graph.groups().size() == 1);
    const auto again = writeProject(second.value());
    REQUIRE(again.ok());
    CHECK(text.value() == again.value());
}

TEST_CASE("Gruppe in der Datei: ohne expanded ist sie zugeklappt, ohne Gruppen ändert sich nichts", "[project][group]") {
    const Library lib;
    const auto collapsed = parseProject(kNodes + "nodegroup G\n  member n1\nend\noutput n3.out\n", lib.registry);
    REQUIRE(collapsed.ok());
    CHECK(collapsed.value().graph.groups().front().collapsed);
    const auto plain = parseProject(kNodes + "output n3.out\n", lib.registry);
    REQUIRE(plain.ok());
    const auto text = writeProject(plain.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("nodegroup") == std::string::npos);
}

TEST_CASE("Gruppe in der Datei: Fehler nennen die Zeile", "[project][group]") {
    const Library lib;
    const auto bad = [&](const std::string& body) { return parseProject(kNodes + body + "output n3.out\n", lib.registry); };

    auto unknownNode = bad("nodegroup G\n  member n9\nend\n");
    REQUIRE_FALSE(unknownNode.ok());
    CHECK(unknownNode.error().code == ErrorCode::NotFound);
    CHECK(unknownNode.error().message.find("Zeile") != std::string::npos);

    CHECK_FALSE(bad("nodegroup G\n  member n1\n  port n1.gibtsnicht as X\nend\n").ok());
    CHECK_FALSE(bad("nodegroup G\n  member n1\n  port n1.out Ausgang\nend\n").ok());   // "as" fehlt
    CHECK_FALSE(bad("nodegroup G\n  member n1\n  param n1.speed\nend\n").ok());         // Beschriftung fehlt
    CHECK_FALSE(bad("nodegroup G\n  frob\nend\n").ok());
    CHECK_FALSE(bad("nodegroup G\n  member n1\n").ok());                              // end fehlt
    CHECK_FALSE(bad("nodegroup G\n  member n1\nend\nnodegroup G\n  member n2\nend\n").ok());   // Name doppelt
    CHECK_FALSE(bad("nodegroup A\n  member n1\nend\nnodegroup B\n  member n1\nend\n").ok());   // Node in zwei Gruppen
}

TEST_CASE("Gruppe in der Datei: auch im look-Block", "[project][group]") {
    const Library lib;
    const std::string text = kNodes + "output n3.out\n\nlook \"Blau\"\n" +
                             "node n1 wh.generator.shapes\nend\nnodegroup Innen\n  member n1\nend\noutput n1.out\nendlook\n";
    const auto parsed = parseProject(text, lib.registry);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().looks.size() == 1);
    CHECK(parsed.value().looks.front().graph.groups().size() == 1);
    CHECK(parsed.value().graph.groups().empty());
    const auto written = writeProject(parsed.value());
    REQUIRE(written.ok());
    const auto again = parseProject(written.value(), lib.registry);
    REQUIRE(again.ok());
    CHECK(again.value().looks.front().graph.groups().size() == 1);
}
