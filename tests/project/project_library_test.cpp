// Tests: Benutzerbibliothek der Bauformen als Textdatei (Schreiben, Lesen, Rundreise, Fehler).
#include <catch2/catch_test_macros.hpp>

#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;

TEST_CASE("Bibliothek: Rundreise mit allen Formen", "[project][library]") {
    auto library = nodes::BuiltinLibrary::create().value();
    std::vector<project::FixtureType> types;
    mapping::MatrixShape matrix;
    matrix.columns = 5;
    matrix.rows = 3;
    types.push_back({"Kachel 5x3", matrix});
    mapping::RingShape ring;
    ring.count = 24;
    ring.radius = 4.0F;
    types.push_back({"Kreis \"groß\"", ring});
    mapping::HelixShape helix;
    helix.count = 60;
    types.push_back({"Säule", helix});
    mapping::StripShape strip;
    strip.count = 30;
    strip.end = Vec3{30.0F, 0.0F, 0.0F};
    types.push_back({"Strip", strip});

    const std::string text = project::writeFixtureLibrary(types);
    CHECK(text.rfind("whitehole-project 1", 0) == 0);
    const auto parsed = project::parseFixtureLibrary(text, library->registry);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().size() == 4);
    CHECK(parsed.value()[1].name == "Kreis \"groß\"");
    CHECK(std::get<mapping::MatrixShape>(parsed.value()[0].shape).columns == 5);
    CHECK(std::get<mapping::RingShape>(parsed.value()[1].shape).count == 24);
    CHECK(std::get<mapping::HelixShape>(parsed.value()[2].shape).count == 60);
    CHECK(mapping::ledCount(parsed.value()[3].shape) == 30);
}

TEST_CASE("Bibliothek: leer, doppelter Name und Müll", "[project][library]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto empty = project::parseFixtureLibrary(project::writeFixtureLibrary({}), library->registry);
    REQUIRE(empty.ok());
    CHECK(empty.value().empty());
    const std::string twice = "whitehole-project 1\nfixturetype A\n  matrix 2 2\nend\nfixturetype A\n  matrix 1 1\nend\n";
    CHECK_FALSE(project::parseFixtureLibrary(twice, library->registry).ok());
    CHECK_FALSE(project::parseFixtureLibrary("kein projekt", library->registry).ok());
}

TEST_CASE("Bibliothek: Blöcke ohne Kopfzeile passen in ein neues Projekt", "[project][library]") {
    auto library = nodes::BuiltinLibrary::create().value();
    mapping::RingShape ring;
    const std::string blocks = project::writeFixtureTypes({{"Kreis", ring}});
    const std::string text = "whitehole-project 1\n" + blocks;
    const auto parsed = project::parseProject(text, library->registry);
    REQUIRE(parsed.ok());
    CHECK(parsed.value().types.size() == 1);
}

TEST_CASE("Controller-Bibliothek: Rundreise, leere Datei, Name fehlt oder doppelt", "[project][library]") {
    auto library = nodes::BuiltinLibrary::create().value();
    std::vector<output::Destination> controllers;
    controllers.emplace_back("10.0.0.7", 6455, output::Protocol::Sacn, "Wand ESP");
    controllers.emplace_back("", 0, output::Protocol::Sacn, "Multicast \"alle\"");
    controllers.emplace_back("192.168.1.50", 0, output::Protocol::Ddp, "Säule");

    const std::string text = project::writeControllerLibrary(controllers);
    CHECK(text.rfind("whitehole-project 1", 0) == 0);
    const auto parsed = project::parseControllerLibrary(text, library->registry);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().size() == 3);
    CHECK(parsed.value()[0].name == "Wand ESP");
    CHECK(parsed.value()[0].port == 6455);
    CHECK(parsed.value()[0].protocol == output::Protocol::Sacn);
    CHECK(parsed.value()[1].address.empty());
    CHECK(parsed.value()[1].name == "Multicast \"alle\"");
    CHECK(parsed.value()[2].protocol == output::Protocol::Ddp);

    CHECK(project::parseControllerLibrary(project::writeControllerLibrary({}), library->registry).value().empty());

    std::vector<output::Destination> unnamed{output::Destination{"10.0.0.7", 0}};
    CHECK_FALSE(project::parseControllerLibrary(project::writeControllerLibrary(unnamed), library->registry).ok());
    std::vector<output::Destination> twice;
    twice.emplace_back("10.0.0.7", 0, output::Protocol::ArtNet, "A");
    twice.emplace_back("10.0.0.8", 0, output::Protocol::ArtNet, "A");
    CHECK_FALSE(project::parseControllerLibrary(project::writeControllerLibrary(twice), library->registry).ok());
    CHECK_FALSE(project::parseControllerLibrary("kein Projekt", library->registry).ok());
}
