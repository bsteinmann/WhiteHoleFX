// example_projects_test.cpp: die Beispielprojekte in examples/ laden, haben ein Ziel und kompilieren.
//
// Warum es den Test gibt: Beispiele veralten still, wenn ein Node umbenannt oder ein Parameter
// gestrichen wird. Hier fällt das beim nächsten Testlauf auf. Die Dateien werden relativ zu diesem
// Quelltext gesucht, brauchen also keine Installation.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "builtin_lab.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using whitehole::test::Lab;

namespace {

void checkExample(const char* name, const char* expectedNode) {
    const auto path = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / name;
    const auto text = project::readTextFile(path.string());
    REQUIRE(text.ok());
    Lab lab;
    auto parsed = project::parseProject(text.value(), lab.registry);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().target.has_value());
    const auto compiled = compiler::compileField(parsed.value().graph, lab.snippets, *parsed.value().target);
    REQUIRE(compiled.ok());
    // Der Effekt, den das Beispiel zeigen soll, steckt wirklich im Shader (Funktion je Node im WGSL)
    CHECK(compiled.value().wgsl.find(expectedNode) != std::string::npos);
}

}  // namespace

TEST_CASE("Beispiel: examples/effekte/gewitter.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("effekte/gewitter.whfx", "wh_bolt_x");
}

TEST_CASE("Beispiel: examples/effekte/funken.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("effekte/funken.whfx", "wh_seg_dist");
}

TEST_CASE("Beispiel: examples/effekte/laufschrift.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("effekte/laufschrift.whfx", "wh_text_glyph");
}

TEST_CASE("Beispiel: examples/audio/takt.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("audio/takt.whfx", "vec4<f32>");
}

TEST_CASE("Beispiel: examples/effekte/kaleidoskop.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("effekte/kaleidoskop.whfx", "atan2");
}

TEST_CASE("Beispiel: examples/audio/oszilloskop.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("audio/oszilloskop.whfx", "wh_scope_dist");
}

TEST_CASE("Beispiel: examples/einstieg/looks.whfx hat drei Looks, die alle kompilieren", "[examples][nodes][looks]") {
    const auto path = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / "einstieg" / "looks.whfx";
    const auto text = project::readTextFile(path.string());
    REQUIRE(text.ok());
    Lab lab;
    auto parsed = project::parseProject(text.value(), lab.registry);
    REQUIRE(parsed.ok());
    const auto& looks = parsed.value().looks;
    REQUIRE(looks.size() == 3);
    CHECK(looks[0].name == "Plasma");
    CHECK(looks[1].name == "Regen");
    CHECK(looks[2].fadeSeconds == 1.5);
    for (const project::Look& look : looks) {
        const auto compiled = compiler::compileField(look.graph, lab.snippets, look.target);
        CHECK(compiled.ok());
    }
    // Schreiben und wieder Lesen verliert nichts
    const auto written = project::writeProject(parsed.value());
    REQUIRE(written.ok());
    auto again = project::parseProject(written.value(), lab.registry);
    REQUIRE(again.ok());
    CHECK(again.value().looks.size() == 3);
}

TEST_CASE("Beispiele: jede Datei in examples/ (auch in Untergruppen) kompiliert", "[examples][nodes]") {
    const auto dir = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples";
    Lab lab;
    std::size_t found = 0;
    std::size_t threeD = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator{dir}) {
        if (entry.path().extension() != ".whfx") {
            continue;
        }
        ++found;
        if (entry.path().parent_path().filename() == "3d") {
            ++threeD;
        }
        INFO(entry.path().string());
        const auto text = project::readTextFile(entry.path().string());
        REQUIRE(text.ok());
        auto parsed = project::parseProject(text.value(), lab.registry);
        REQUIRE(parsed.ok());
        REQUIRE(parsed.value().target.has_value());
        const auto compiled = compiler::compileField(parsed.value().graph, lab.snippets, *parsed.value().target);
        CHECK(compiled.ok());
        for (const project::Look& look : parsed.value().looks) {
            CHECK(compiler::compileField(look.graph, lab.snippets, look.target).ok());
        }
    }
    CHECK(found >= 15);
    CHECK(threeD == 5);  // die fünf 3D-Beispiele in examples/3d/
}
