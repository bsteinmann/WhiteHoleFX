// Tests von Gruppen, Regionen und Output-Zielen in der Projektdatei (Rundlauf und Fehlerfälle).
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;

namespace {

struct Library {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    Library() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }
};

const std::string kText = R"(whitehole-project 1
destination 0 127.0.0.1
fixture "Matrix 1"
  matrix 4 2
  patch artnet destination 0 universe 0
end
fixture "Matrix 2"
  matrix 4 2
  patch artnet destination 0 universe 1
end
group "Bühne links" "Matrix 1" "Matrix 2"
region Mitte 0.25 0.5 0.5 0.25
node a wh.output.canvas
  target fixture "Matrix 1"
  target group "Bühne links"
  target region Mitte
end
output a.out
)";

}  // namespace

TEST_CASE("Gruppen, Regionen und Ziele überstehen den Rundlauf", "[project][target]") {
    Library lib;
    const Result<project::Project> parsed = project::parseProject(kText, lib.registry);
    REQUIRE(parsed.ok());
    const project::Project& p = parsed.value();
    REQUIRE(p.groups.size() == 1);
    CHECK(p.groups[0].name == "Bühne links");
    CHECK(p.groups[0].fixtures == std::vector<std::string>{"Matrix 1", "Matrix 2"});
    REQUIRE(p.regions.size() == 1);
    CHECK(p.regions[0].left == 0.25F);
    CHECK(p.regions[0].height == 0.25F);
    REQUIRE(p.graph.nodes().size() == 1);
    CHECK(p.graph.nodes()[0].targets ==
          std::vector<std::string>{"fixture:Matrix 1", "group:Bühne links", "region:Mitte"});

    const Result<std::string> text = project::writeProject(p);
    REQUIRE(text.ok());
    const Result<project::Project> again = project::parseProject(text.value(), lib.registry);
    REQUIRE(again.ok());
    CHECK(again.value().groups.size() == 1);
    CHECK(again.value().regions.size() == 1);
    CHECK(again.value().graph.nodes()[0].targets == p.graph.nodes()[0].targets);
}

TEST_CASE("Fehlerhafte Gruppen, Regionen und Ziele werden mit Zeile abgelehnt", "[project][target]") {
    Library lib;
    const std::string head = "whitehole-project 1\n";
    CHECK_FALSE(project::parseProject(head + "group Leer\n", lib.registry).ok());
    CHECK_FALSE(project::parseProject(head + "group A x\ngroup A y\n", lib.registry).ok());
    CHECK_FALSE(project::parseProject(head + "region R 0 0 0 1\n", lib.registry).ok());
    CHECK_FALSE(project::parseProject(head + "region R 0 0 2 1\n", lib.registry).ok());
    CHECK_FALSE(project::parseProject(head + "node a wh.output.canvas\n  target wand X\nend\n", lib.registry).ok());
    CHECK_FALSE(project::parseProject(head + "look L fade 1\ngroup A x\nendlook\n", lib.registry).ok());
}

TEST_CASE("setTargets entfernt Duplikate und lehnt leere Einträge ab", "[graph][target]") {
    Library lib;
    graph::Graph graph{lib.registry};
    const NodeId id = graph.addNode("wh.output.canvas", Vec2{}).value();
    REQUIRE(graph.setTargets(id, {"fixture:A", "fixture:A", "group:B"}).ok());
    CHECK(graph.findNode(id)->targets == std::vector<std::string>{"fixture:A", "group:B"});
    CHECK_FALSE(graph.setTargets(id, {""}).ok());
    CHECK_FALSE(graph.setTargets(NodeId{9999}, {"fixture:A"}).ok());
}

#include <cmath>

#include "whitehole/project/output_target.hpp"

namespace {

bool near(float a, float b) { return std::fabs(a - b) < 0.02F; }

}  // namespace

TEST_CASE("Das Ziel-Rechteck umschließt Fixture, Gruppe und Region", "[project][target]") {
    Library lib;
    // Zwei Matrizen nebeneinander: links Matrix 1 (x 0..3), rechts Matrix 2 (x 10..13), gleiche Höhe.
    const std::string text = R"(whitehole-project 1
fixture "Matrix 1" at 0 0 0
  matrix 4 4 pitch 1 1
end
fixture "Matrix 2" at 10 0 0
  matrix 4 4 pitch 1 1
end
group Beide "Matrix 1" "Matrix 2"
region Oben 0 0 1 0.25
node a wh.output.canvas
  target fixture "Matrix 2"
end
output a.out
)";
    Result<project::Project> parsed = project::parseProject(text, lib.registry);
    REQUIRE(parsed.ok());
    project::Project& p = parsed.value();
    const Result<mapping::LedMap> map = p.layout.bake(p.normalize);
    REQUIRE(map.ok());

    const auto right = project::resolveTargets(p, map.value(), {"fixture:Matrix 2"});
    REQUIRE(right.ok());
    CHECK(right.value().rect.left > 0.5F);
    CHECK(near(right.value().rect.left + right.value().rect.width, 1.0F));
    CHECK(near(right.value().rect.height, 1.0F));

    const auto left = project::resolveTargets(p, map.value(), {"fixture:Matrix 1"});
    REQUIRE(left.ok());
    CHECK(near(left.value().rect.left, 0.0F));
    CHECK(left.value().rect.width < 0.5F);

    const auto both = project::resolveTargets(p, map.value(), {"group:Beide"});
    REQUIRE(both.ok());
    CHECK(near(both.value().rect.left, 0.0F));
    CHECK(near(both.value().rect.width, 1.0F));

    const auto region = project::resolveTargets(p, map.value(), {"region:Oben"});
    REQUIRE(region.ok());
    CHECK(near(region.value().rect.top, 0.0F));
    CHECK(near(region.value().rect.height, 0.25F));

    const auto missing = project::resolveTargets(p, map.value(), {"fixture:Gibt es nicht", "fixture:Matrix 1"});
    REQUIRE(missing.ok());
    CHECK(missing.value().unknown == std::vector<std::string>{"fixture:Gibt es nicht"});
    CHECK_FALSE(project::resolveTargets(p, map.value(), {"group:Nix"}).ok());

    // Anwenden schreibt die Parameter des Output-Nodes.
    const std::vector<std::string> unknown = project::applyOutputTargets(p, map.value());
    CHECK(unknown.empty());
    const auto& params = p.graph.nodes()[0].parameters;
    CHECK(near(static_cast<float>(std::get<double>(params.at("left"))), right.value().rect.left));
    CHECK(near(static_cast<float>(std::get<double>(params.at("width"))), right.value().rect.width));
}
