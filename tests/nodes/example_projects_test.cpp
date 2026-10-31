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

TEST_CASE("Beispiel: examples/gewitter.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("gewitter.whfx", "wh_bolt_x");
}

TEST_CASE("Beispiel: examples/funken.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("funken.whfx", "wh_seg_dist");
}

TEST_CASE("Beispiel: examples/laufschrift.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("laufschrift.whfx", "wh_text_glyph");
}

TEST_CASE("Beispiel: examples/takt.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("takt.whfx", "vec4<f32>");
}

TEST_CASE("Beispiel: examples/kaleidoskop.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("kaleidoskop.whfx", "atan2");
}

TEST_CASE("Beispiel: examples/oszilloskop.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("oszilloskop.whfx", "wh_scope_dist");
}
