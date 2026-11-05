// Tests der Fixture-Vorlagen: Rundlauf von Form, Verkabelung und Farbeinstellungen; Fehlerfälle.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <variant>

#include "whitehole/project/fixture_template.hpp"

using namespace whitehole;
using project::FixtureTemplate;

TEST_CASE("Eine Matrix-Vorlage übersteht den Rundlauf", "[project][template]") {
    mapping::MatrixShape m;
    m.columns = 5;
    m.rows = 5;
    m.pitch = Vec2{2.0F, 3.0F};
    m.wiring.serpentine = false;
    m.wiring.start = mapping::StartCorner::TopRight;
    FixtureTemplate tpl;
    tpl.name = "Matrix 5x5 weit";
    tpl.shape = m;
    output::FixturePatch patch;
    patch.protocol = output::Protocol::Sacn;
    patch.color.order = output::ColorOrder::GRB;
    patch.color.gamma = 1.8F;
    patch.color.brightness = 0.5F;
    tpl.patch = patch;

    const Result<std::string> text = project::writeFixtureTemplate(tpl);
    REQUIRE(text.ok());
    const Result<FixtureTemplate> back = project::parseFixtureTemplate(text.value());
    REQUIRE(back.ok());
    CHECK(back.value().name == "Matrix 5x5 weit");
    const auto* shape = std::get_if<mapping::MatrixShape>(&back.value().shape);
    REQUIRE(shape != nullptr);
    CHECK(shape->columns == 5);
    CHECK(shape->rows == 5);
    CHECK(shape->pitch.x == 2.0F);
    CHECK(shape->wiring.serpentine == false);
    CHECK(shape->wiring.start == mapping::StartCorner::TopRight);
    REQUIRE(back.value().patch.has_value());
    CHECK(back.value().patch->protocol == output::Protocol::Sacn);
    CHECK(back.value().patch->color.order == output::ColorOrder::GRB);
    CHECK(back.value().patch->color.gamma == 1.8F);
}

TEST_CASE("Ein Strip ohne Patch bleibt ohne Patch", "[project][template]") {
    mapping::StripShape s;
    s.count = 78;
    s.end = Vec3{77.0F, 0.0F, 0.0F};
    FixtureTemplate tpl;
    tpl.name = "Band 78";
    tpl.shape = s;
    const Result<std::string> text = project::writeFixtureTemplate(tpl);
    REQUIRE(text.ok());
    const Result<FixtureTemplate> back = project::parseFixtureTemplate(text.value());
    REQUIRE(back.ok());
    const auto* shape = std::get_if<mapping::StripShape>(&back.value().shape);
    REQUIRE(shape != nullptr);
    CHECK(shape->count == 78);
    CHECK_FALSE(back.value().patch.has_value());
}

TEST_CASE("Vorlagen ohne Namen oder mit mehreren Fixtures werden abgelehnt", "[project][template]") {
    FixtureTemplate unnamed;
    CHECK_FALSE(project::writeFixtureTemplate(unnamed).ok());
    CHECK_FALSE(project::parseFixtureTemplate("whitehole-project 1\n").ok());
    CHECK_FALSE(project::parseFixtureTemplate("kein projekt").ok());
}

TEST_CASE("Die Bibliothek speichert, listet sortiert, ersetzt und löscht", "[project][template]") {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "whfx_fixture_library_test";
    std::filesystem::remove_all(dir);
    project::FixtureLibrary library{dir};
    CHECK(library.list().empty());

    FixtureTemplate b;
    b.name = "Band: 78/Pixel";
    mapping::StripShape s;
    s.count = 78;
    s.end = Vec3{77.0F, 0.0F, 0.0F};
    b.shape = s;
    FixtureTemplate a;
    a.name = "Alpha";
    REQUIRE(library.save(b).ok());
    REQUIRE(library.save(a).ok());
    s.count = 60;
    s.end = Vec3{59.0F, 0.0F, 0.0F};
    b.shape = s;
    REQUIRE(library.save(b).ok());  // ersetzt

    const std::vector<FixtureTemplate> all = library.list();
    REQUIRE(all.size() == 2);
    CHECK(all[0].name == "Alpha");
    CHECK(all[1].name == "Band: 78/Pixel");
    CHECK(std::get<mapping::StripShape>(all[1].shape).count == 60);

    CHECK(library.remove("Alpha").ok());
    CHECK_FALSE(library.remove("Alpha").ok());
    CHECK(library.list().size() == 1);
    std::filesystem::remove_all(dir);
}
