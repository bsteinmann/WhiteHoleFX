// Tests: Protokoll gehört zum Controller (destination ... protocol ...), Altdateien werden umgesetzt.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using output::Protocol;

namespace {

std::string fixtureBlock(const std::string& name, const std::string& patch) {
    return "fixture " + name + "\n  matrix 4 4 pitch 1 1\n  " + patch + "\nend\n";
}

Result<project::Project> parse(const std::string& text) {
    static auto library = nodes::BuiltinLibrary::create().value();
    return project::parseProject(text, library->registry);
}

}  // namespace

TEST_CASE("Controller: neue Schreibweise mit Port, Protokoll und Name", "[project][destinations]") {
    const auto parsed = parse("whitehole-project 1\ndestination 0 10.0.0.7 6455 protocol sacn name \"Wand ESP\"\n" +
                              fixtureBlock("A", "patch sacn destination 0 universe 3 channel 1"));
    REQUIRE(parsed.ok());
    const output::Destination& d = parsed.value().destinations[0];
    CHECK(d.address == "10.0.0.7");
    CHECK(d.port == 6455);
    CHECK(d.protocol == Protocol::Sacn);
    CHECK(d.name == "Wand ESP");
}

TEST_CASE("Controller: Altdatei bekommt das Protokoll seiner Patches", "[project][destinations]") {
    const auto parsed = parse("whitehole-project 1\ndestination 0 10.0.0.7\ndestination 1 multicast\ndestination 2 10.0.0.9\n" +
                              fixtureBlock("A", "patch artnet destination 0 universe 0 channel 1") +
                              fixtureBlock("B", "patch sacn destination 1 universe 1 channel 1"));
    REQUIRE(parsed.ok());
    const auto& destinations = parsed.value().destinations;
    REQUIRE(destinations.size() == 3);
    CHECK(destinations[0].protocol == Protocol::ArtNet);
    CHECK(destinations[1].protocol == Protocol::Sacn);
    CHECK(destinations[2].protocol == Protocol::ArtNet);  // ohne Patch: Art-Net, mit leerer Adresse sACN
}

TEST_CASE("Controller: Altdatei mit zwei Protokollen auf einer Adresse wird in zwei Controller geteilt", "[project][destinations]") {
    const auto parsed = parse("whitehole-project 1\ndestination 0 10.0.0.7\n" +
                              fixtureBlock("A", "patch artnet destination 0 universe 0 channel 1") +
                              fixtureBlock("B", "patch ddp destination 0 offset 0"));
    REQUIRE(parsed.ok());
    const project::Project& p = parsed.value();
    REQUIRE(p.destinations.size() == 2);
    CHECK(p.destinations[0].protocol == Protocol::ArtNet);
    CHECK(p.destinations[1].protocol == Protocol::Ddp);
    CHECK(p.destinations[1].address == "10.0.0.7");
    for (const output::FixturePatch& patch : p.patches) {
        CHECK(patch.destination == (patch.protocol == Protocol::Ddp ? 1U : 0U));
    }
}

TEST_CASE("Controller: Protokoll beim Ziel und anderes im Patch ist ein Fehler mit Fixture-Namen", "[project][destinations]") {
    const auto parsed = parse("whitehole-project 1\ndestination 0 10.0.0.7 protocol sacn\n" +
                              fixtureBlock("Wand", "patch artnet destination 0 universe 0 channel 1"));
    REQUIRE_FALSE(parsed.ok());
    CHECK(parsed.error().message.find("Wand") != std::string::npos);
}

TEST_CASE("Controller: Schreiben und Lesen ergibt dasselbe", "[project][destinations]") {
    auto parsed = parse("whitehole-project 1\ndestination 0 10.0.0.7 protocol ddp name \"Säule\"\n" +
                        fixtureBlock("A", "patch ddp destination 0 offset 0"));
    REQUIRE(parsed.ok());
    const auto text = project::writeProject(parsed.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("protocol ddp name") != std::string::npos);
    const auto again = parse(text.value());
    REQUIRE(again.ok());
    CHECK(again.value().destinations[0].name == "Säule");
    CHECK(again.value().destinations[0].protocol == Protocol::Ddp);
}

TEST_CASE("Controller: alignPatches stellt Patches um und verschiebt das Universe", "[project][destinations]") {
    auto parsed = parse("whitehole-project 1\ndestination 0 10.0.0.7\n" +
                        fixtureBlock("A", "patch artnet destination 0 universe 4 channel 1"));
    REQUIRE(parsed.ok());
    project::Project p = std::move(parsed.value());
    p.destinations[0].protocol = Protocol::Sacn;
    const auto changed = project::alignPatches(p);
    REQUIRE(changed.ok());
    CHECK(changed.value() == 1);
    CHECK(p.patches[0].protocol == Protocol::Sacn);
    CHECK(p.patches[0].universe == 5);
    CHECK(project::alignPatches(p).value() == 0);
}
