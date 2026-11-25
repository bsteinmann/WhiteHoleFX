// Standbild auf dem Look-Knopf: Verkleinern, Aufnahme beim Speichern und `thumb` in der Projektdatei.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/look_thumbnail.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
end
output plasma.out
)";

std::string hexOf(std::size_t pixels, const std::string& rgb) {
    std::string out;
    for (std::size_t i = 0; i < pixels; ++i) {
        out += rgb;
    }
    return out;
}

}  // namespace

TEST_CASE("makeLookThumbnail mittelt Blöcke und legt Alpha über Schwarz", "[live][thumbnail]") {
    preview::Image image;
    image.width = 4;
    image.height = 2;
    image.rgba.assign(4 * 2 * 4, 0);
    const auto set = [&](std::uint32_t x, std::uint32_t y, std::uint8_t r, std::uint8_t a) {
        const std::size_t at = (static_cast<std::size_t>(y) * 4 + x) * 4;
        image.rgba[at] = r;
        image.rgba[at + 3] = a;
    };
    set(0, 0, 200, 255);
    set(1, 0, 100, 255);
    set(0, 1, 100, 255);
    set(1, 1, 0, 255);
    set(2, 0, 255, 0);  // durchsichtig: zählt als Schwarz
    const auto thumb = runtime::makeLookThumbnail(image, 2, 1);
    REQUIRE(thumb.has_value());
    CHECK(thumb->width == 2);
    CHECK(thumb->height == 1);
    REQUIRE(thumb->rgb.size() == 6);
    CHECK(thumb->rgb[0] == 100);  // (200+100+100+0)/4
    CHECK(thumb->rgb[3] == 0);

    CHECK_FALSE(runtime::makeLookThumbnail(image, 8, 4).has_value());  // Bild kleiner als das Ziel
    CHECK_FALSE(runtime::makeLookThumbnail(image, 0, 1).has_value());
    CHECK_FALSE(runtime::makeLookThumbnail(preview::Image{}, 1, 1).has_value());
}

TEST_CASE("Look speichern nimmt ein Standbild auf, die Projektdatei behält es", "[live][thumbnail]") {
    auto library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    REQUIRE(player.loadText(kProject).ok());
    REQUIRE(player.start().ok());
    REQUIRE(player.step().ok());

    const auto index = player.saveLook("Neu", 1.0);
    REQUIRE(index.ok());
    const project::Look& look = player.looks()[static_cast<std::size_t>(index.value())];
    REQUIRE(look.thumbnail.has_value());
    CHECK(look.thumbnail->width == runtime::kThumbnailWidth);
    CHECK(look.thumbnail->height == runtime::kThumbnailHeight);
    CHECK(look.thumbnail->rgb.size() == runtime::kThumbnailWidth * runtime::kThumbnailHeight * 3);

    const auto text = project::writeProject(*player.project());
    REQUIRE(text.ok());
    CHECK(text.value().find("\n  thumb 96 54 ") != std::string::npos);
    const auto again = project::parseProject(text.value(), library->registry);
    REQUIRE(again.ok());
    REQUIRE(again.value().looks.size() == 1);
    REQUIRE(again.value().looks[0].thumbnail.has_value());
    CHECK(again.value().looks[0].thumbnail->rgb == look.thumbnail->rgb);

    // Überschreiben ersetzt das Standbild, ohne das Look-Verhalten anzufassen.
    REQUIRE(player.saveLook("Neu", 1.0).ok());
    CHECK(player.looks()[0].thumbnail.has_value());
}

TEST_CASE("thumb in der Projektdatei: Fehler und ältere Dateien", "[live][thumbnail]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto parse = [&](const std::string& thumbLine) {
        const std::string text = std::string{"whitehole-project 1\nlook X fade 1\n"} + thumbLine +
                                 "\nnode n wh.generator.plasma\nend\noutput n.out\nendlook\n";
        return project::parseProject(text, library->registry);
    };
    const auto good = parse("  thumb 2 1 " + hexOf(2, "0a1B2c"));
    REQUIRE(good.ok());
    REQUIRE(good.value().looks[0].thumbnail.has_value());
    CHECK(good.value().looks[0].thumbnail->rgb[0] == 0x0a);
    CHECK(good.value().looks[0].thumbnail->rgb[1] == 0x1b);
    CHECK(parse("").value().looks[0].thumbnail.has_value() == false);  // ohne Zeile: kein Standbild

    CHECK_FALSE(parse("  thumb 2 1 " + hexOf(1, "0a1b2c")).ok());  // zu kurz
    CHECK_FALSE(parse("  thumb 2 1 " + hexOf(2, "zz1b2c")).ok());  // kein Hex
    CHECK_FALSE(parse("  thumb 0 1 ").ok());
    CHECK_FALSE(parse("  thumb 129 1 00").ok());
    CHECK_FALSE(parse("  thumb 1 1").ok());
    CHECK_FALSE(project::parseProject("whitehole-project 1\nthumb 1 1 000000\n", library->registry).ok());  // außerhalb
}
