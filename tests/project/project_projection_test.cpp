// Projektdatei: `projection volume|faces|box` in der fixture-Kopfzeile (lesen, schreiben, Fehler, Standard).
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
using mapping::Projection;

namespace {

struct Library {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    Library() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }
};

const char* const kProject = R"(whitehole-project 1
fixture "Vorne" at 0 0 0 projection box
  matrix 4 4
end
fixture "Hinten" at 0 0 4 rotate 0 180 0 projection faces
  matrix 4 4
end
fixture "Frei" at 10 0 0
  matrix 2 2
end
)";

}  // namespace

TEST_CASE("Projektdatei: projection wird je Fixture gelesen, ohne Angabe gilt volume", "[project][projection]") {
    const Library lib;
    const auto project = parseProject(kProject, lib.registry);
    REQUIRE(project.ok());
    const auto& fixtures = project.value().layout.fixtures();
    REQUIRE(fixtures.size() == 3);
    CHECK(fixtures[0].projection == Projection::Box);
    CHECK(fixtures[1].projection == Projection::Faces);
    CHECK(fixtures[2].projection == Projection::Volume);
    CHECK(fixtures[1].rotation.y == 180.0F);  // andere Optionen der Kopfzeile bleiben unberührt
}

TEST_CASE("Projektdatei: projection wird geschrieben, volume bleibt weg, der Rundlauf ist stabil", "[project][projection]") {
    const Library lib;
    const auto first = parseProject(kProject, lib.registry);
    REQUIRE(first.ok());
    const auto text = writeProject(first.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("projection box") != std::string::npos);
    CHECK(text.value().find("projection faces") != std::string::npos);
    CHECK(text.value().find("projection volume") == std::string::npos);

    const auto second = parseProject(text.value(), lib.registry);
    REQUIRE(second.ok());
    const auto& fixtures = second.value().layout.fixtures();
    REQUIRE(fixtures.size() == 3);
    CHECK(fixtures[0].projection == Projection::Box);
    CHECK(fixtures[1].projection == Projection::Faces);
    CHECK(fixtures[2].projection == Projection::Volume);
    const auto again = writeProject(second.value());
    REQUIRE(again.ok());
    CHECK(again.value() == text.value());
}

TEST_CASE("Projektdatei: ein unbekannter Wert nennt die gültigen Namen", "[project][projection]") {
    const Library lib;
    const auto result = parseProject("whitehole-project 1\nfixture A projection kugel\n  matrix 2 2\nend\n", lib.registry);
    REQUIRE_FALSE(result.ok());
    const std::string& message = result.error().message;
    CHECK(message.find("volume") != std::string::npos);
    CHECK(message.find("faces") != std::string::npos);
    CHECK(message.find("box") != std::string::npos);
}

TEST_CASE("Projektdatei: ein Fixture mit type und projection behält beides", "[project][projection]") {
    const Library lib;
    const auto result = parseProject(
        "whitehole-project 1\nfixturetype Panel\n  matrix 4 4\nend\nfixture A at 0 0 0 projection box type Panel\nend\n", lib.registry);
    REQUIRE(result.ok());
    REQUIRE(result.value().layout.fixtures().size() == 1);
    CHECK(result.value().layout.fixtures()[0].projection == Projection::Box);
    const auto text = writeProject(result.value());
    REQUIRE(text.ok());
    const auto again = parseProject(text.value(), lib.registry);
    REQUIRE(again.ok());
    CHECK(again.value().layout.fixtures()[0].projection == Projection::Box);
}
