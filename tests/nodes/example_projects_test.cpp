// example_projects_test.cpp: die Beispielprojekte in examples/ laden, haben ein Ziel und kompilieren.
//
// Warum es den Test gibt: Beispiele veralten still, wenn ein Node umbenannt oder ein Parameter
// gestrichen wird. Hier fällt das beim nächsten Testlauf auf. Die Dateien werden relativ zu diesem
// Quelltext gesucht, brauchen also keine Installation.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
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

TEST_CASE("Beispiel: examples/effekte/schlag-kreis.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("effekte/schlag-kreis.whfx", "wh_");
}

TEST_CASE("Beispiel: examples/effekte/schuss.whfx lädt und kompiliert", "[examples][nodes]") {
    checkExample("effekte/schuss.whfx", "wh_");
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

TEST_CASE("Beispiel: examples/einstieg/cuelist.whfx hat vier Cues mit allen Auslöser-Arten, die überleben Schreiben", "[examples][nodes][cues]") {
    const auto path = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / "einstieg" / "cuelist.whfx";
    const auto text = project::readTextFile(path.string());
    REQUIRE(text.ok());
    Lab lab;
    auto parsed = project::parseProject(text.value(), lab.registry);
    REQUIRE(parsed.ok());
    const auto& cues = parsed.value().cues;
    REQUIRE(cues.size() == 4);
    CHECK(cues[0].look == "Plasma");
    CHECK(cues[1].trigger == project::CueTrigger::Time);
    CHECK(cues[1].time == 6.0);
    CHECK(cues[2].trigger == project::CueTrigger::Key);
    CHECK(cues[2].key == "F5");
    CHECK(cues[2].delay == 1.5);
    CHECK(cues[3].look.empty());
    const auto written = project::writeProject(parsed.value());
    REQUIRE(written.ok());
    auto again = project::parseProject(written.value(), lab.registry);
    REQUIRE(again.ok());
    CHECK(again.value().cues == cues);
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
    CHECK(threeD == 6);  // die sechs 3D-Beispiele in examples/3d/
}

TEST_CASE("Beispiel: examples/spiele/pong.whfx lädt und kompiliert", "[examples][nodes][pong]") {
    checkExample("spiele/pong.whfx", "wh_pong_cov");
}

TEST_CASE("Beispiel: examples/spiele/pong-takt.whfx lädt und kompiliert", "[examples][nodes][pong]") {
    checkExample("spiele/pong-takt.whfx", "wh_pong_cov");
}

TEST_CASE("Beispiel: examples/3d/wuerfel-hohl.whfx lädt, kompiliert und ist ein hohler Würfel", "[examples][nodes][cube]") {
    checkExample("3d/wuerfel-hohl.whfx", "length(q.xy");  // Plasma-Körper im Shader

    // Geometrie: 6 Flächen zu je 32 x 32 LEDs, alle auf der Außenhaut, keine im Inneren.
    const auto path = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / "3d" / "wuerfel-hohl.whfx";
    const auto text = project::readTextFile(path.string());
    REQUIRE(text.ok());
    Lab lab;
    auto parsed = project::parseProject(text.value(), lab.registry);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().layout.fixtures().size() == 6);
    for (const mapping::Fixture& fixture : parsed.value().layout.fixtures()) {
        CHECK(fixture.projection == mapping::Projection::Box);
    }
    const auto map = parsed.value().layout.bake(parsed.value().normalize);
    REQUIRE(map.ok());
    REQUIRE(map.value().world.size() == 6U * 32U * 32U);
    constexpr float kLow = 0.0F;   // Flächen bei 0 und 32, LEDs der Fläche bei 0,5 bis 31,5
    constexpr float kHigh = 32.0F;
    constexpr float kEps = 1e-3F;
    for (const Vec3& p : map.value().world) {
        const auto onPlane = [&](float v) { return std::abs(v - kLow) < kEps || std::abs(v - kHigh) < kEps; };
        const bool onSkin = onPlane(p.x) || onPlane(p.y) || onPlane(p.z);
        REQUIRE(onSkin);
        const auto inside = [&](float v) { return v > kLow - kEps && v < kHigh + kEps; };
        REQUIRE((inside(p.x) && inside(p.y) && inside(p.z)));
    }
}
