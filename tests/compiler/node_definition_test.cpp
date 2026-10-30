#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "test_field_nodes.hpp"
#include "whitehole/compiler/node_definition.hpp"

using namespace whitehole;
using namespace whitehole::compiler;
using whitehole::test::fieldNode;
using whitehole::test::outPort;

TEST_CASE("SnippetLibrary: Eintragen, Finden und doppelte Typ-ID") {
    SnippetLibrary library;
    CHECK(library.find("x") == nullptr);

    REQUIRE(library.add("x", NodeSnippet{"", "return vec4<f32>(0.0);"}).ok());
    const NodeSnippet* found = library.find("x");
    REQUIRE(found != nullptr);
    CHECK(found->body == "return vec4<f32>(0.0);");

    const Result<void> again = library.add("x", NodeSnippet{});
    REQUIRE_FALSE(again.ok());
    CHECK(again.error().code == ErrorCode::AlreadyExists);
}

TEST_CASE("registerDefinitions trägt Beschreibung und Snippet ein") {
    graph::NodeRegistry registry;
    SnippetLibrary library;
    const std::vector<NodeDefinition> definitions{
        fieldNode("n.one", {outPort("out", graph::PortType::Field)}, {}, "return a;"),
        fieldNode("n.two", {outPort("out", graph::PortType::Field)}, {}, "return b;")};

    REQUIRE(registerDefinitions(definitions, registry, library).ok());

    CHECK(registry.find("n.one") != nullptr);
    CHECK(registry.find("n.two") != nullptr);
    REQUIRE(library.find("n.two") != nullptr);
    CHECK(library.find("n.two")->body == "return b;");
}

TEST_CASE("registerDefinitions bricht beim ersten Fehler ab und lässt Früheres stehen") {
    graph::NodeRegistry registry;
    SnippetLibrary library;

    auto broken = fieldNode("n.broken", {outPort("bad key", graph::PortType::Field)}, {}, "return a;");
    const std::vector<NodeDefinition> definitions{
        fieldNode("n.ok", {outPort("out", graph::PortType::Field)}, {}, "return a;"), broken,
        fieldNode("n.never", {outPort("out", graph::PortType::Field)}, {}, "return a;")};

    const Result<void> result = registerDefinitions(definitions, registry, library);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::InvalidArgument);

    CHECK(registry.find("n.ok") != nullptr);
    CHECK(registry.find("n.broken") == nullptr);
    CHECK(library.find("n.broken") == nullptr);  // kein Snippet ohne Beschreibung
    CHECK(registry.find("n.never") == nullptr);
}
