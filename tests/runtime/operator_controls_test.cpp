// Tests der Bedienpulte: Master und Blackout, LED-Info beim Überfahren, Liste "Zuletzt geöffnet".
// Das FakeGpuDevice liefert Nullen; was an Farben ankommt, stammt vom Testbild.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/led_info.hpp"
#include "whitehole/runtime/player.hpp"
#include "whitehole/runtime/recent_files.hpp"
#include "whitehole/runtime/session.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

// A: 4x2 = 8 LEDs bei x 0..4, Art-Net Universe 3 ab Kanal 10; B: Strip mit 2 LEDs rechts daneben, sACN.
const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
destination 1 multicast
fixture Wand at 0 0 0
  matrix 4 2
  patch artnet destination 0 universe 3 channel 10
end
fixture Strip at 10 0 0
  strip 2 from 0 0 0 to 2 0 0
  patch sacn destination 1 universe 7 channel 1
end
node g wh.generator.gradient
end
output g.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    engine::ManualFrameClock clock{Seconds{1.0 / 40.0}};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    /// Eine Session mit weißem Testbild; liefert die Farben des ersten Frames.
    std::vector<ColorRgba> firstFrame(float master, bool blackout) {
        auto parsed = project::parseProject(kProject, library->registry);
        REQUIRE(parsed.ok());
        runtime::SessionOptions options;
        options.keepLatestColors = true;
        auto session = runtime::Session::create(
            parsed.value(), runtime::SessionDeps{device, library->snippets, library->kernels, recording, clock}, options);
        REQUIRE(session.ok());
        runtime::TestPattern pattern;
        pattern.kind = runtime::TestPatternKind::Color;
        session.value()->setTestPattern(pattern);
        session.value()->setOutputLevel(master, blackout);
        REQUIRE(session.value()->renderFrame().ok());
        REQUIRE(session.value()->flush(std::chrono::seconds{5}));
        return session.value()->latestColors();
    }
};

}  // namespace

TEST_CASE("Master und Blackout: skalieren RGB, auch ein Testbild", "[controls]") {
    Rig rig;
    const std::vector<ColorRgba> full = rig.firstFrame(1.0F, false);
    REQUIRE(full.size() == 10);
    CHECK(full[0] == ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});

    const std::vector<ColorRgba> half = rig.firstFrame(0.5F, false);
    CHECK(half[3] == ColorRgba{0.5F, 0.5F, 0.5F, 1.0F});

    const std::vector<ColorRgba> dark = rig.firstFrame(1.0F, true);  // Blackout gewinnt gegen Master 1
    for (const ColorRgba& c : dark) {
        CHECK(c == ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
    }
}

TEST_CASE("Player: Master wird begrenzt, Blackout und Master überleben Neustart und Laden", "[controls]") {
    Rig rig;
    CHECK(rig.player.masterBrightness() == 1.0F);
    CHECK(rig.player.setMasterBrightness(2.0F) == 1.0F);
    CHECK(rig.player.setMasterBrightness(-1.0F) == 0.0F);
    CHECK(rig.player.setMasterBrightness(std::numeric_limits<float>::quiet_NaN()) == 1.0F);
    CHECK(rig.player.setMasterBrightness(0.25F) == 0.25F);
    rig.player.setBlackout(true);
    REQUIRE(rig.player.start().ok());
    rig.player.stop();
    REQUIRE(rig.player.loadText(kProject).ok());
    CHECK(rig.player.masterBrightness() == 0.25F);
    CHECK(rig.player.blackout());
    CHECK_FALSE(rig.player.modified());
    CHECK_FALSE(rig.player.canUndo());
    rig.player.setBlackout(false);
    CHECK_FALSE(rig.player.blackout());
}

TEST_CASE("LED-Info: Treffer im Bild", "[ledinfo]") {
    Rig rig;
    const auto map = rig.player.project()->layout.bake(rig.player.project()->normalize);
    REQUIRE(map.ok());
    preview::ViewFrame frame;
    frame.scale = 10.0F;  // 10 Pixel je Welteinheit, Ursprung links unten bei (0, 100)
    frame.offsetX = 0.0F;
    frame.offsetY = 100.0F;
    // LED 0 sitzt bei Welt (0.5, 1.5) wegen Zeile 0 oben -> Pixel (5, 85)
    const auto hit = runtime::ledAtPixel(map.value(), frame, 6.0F, 84.0F, 5.0F);
    REQUIRE(hit.has_value());
    CHECK(*hit == 0);
    CHECK_FALSE(runtime::ledAtPixel(map.value(), frame, 60.0F, 10.0F, 5.0F).has_value());
    // weit weg, aber mit großem Radius: die nächste gewinnt
    CHECK(runtime::ledAtPixel(map.value(), frame, 5.0F, 5.0F, 500.0F).has_value());
}

TEST_CASE("LED-Info: Fixture, Nummer am Kabel und Adresse", "[ledinfo]") {
    Rig rig;
    const auto map = rig.player.project()->layout.bake(rig.player.project()->normalize);
    REQUIRE(map.ok());
    const std::vector<ColorRgba> colors(10, ColorRgba{1.0F, 0.5F, 0.0F, 1.0F});

    const auto third = runtime::describeLed(*rig.player.project(), map.value(), 2, colors);
    REQUIRE(third.has_value());
    CHECK(third->fixtureName == "Wand");
    CHECK(third->indexInFixture == 2);
    CHECK(third->fixtureLedCount == 8);
    REQUIRE(third->address.has_value());
    CHECK(third->address->protocol == output::Protocol::ArtNet);
    CHECK(third->address->universe == 3);
    CHECK(third->address->firstChannel == 10 + 2 * 3);
    REQUIRE(third->color.has_value());

    const auto strip = runtime::describeLed(*rig.player.project(), map.value(), 9, {});
    REQUIRE(strip.has_value());
    CHECK(strip->fixtureName == "Strip");
    CHECK(strip->indexInFixture == 1);
    REQUIRE(strip->address.has_value());
    CHECK(strip->address->protocol == output::Protocol::Sacn);
    CHECK(strip->address->universe == 7);
    CHECK(strip->address->firstChannel == 1 + 3);
    CHECK_FALSE(strip->color.has_value());

    CHECK_FALSE(runtime::describeLed(*rig.player.project(), map.value(), 10, colors).has_value());
}

TEST_CASE("LED-Info: ein Fixture über mehrere Universen und ein ungepatchtes", "[ledinfo]") {
    Rig rig;
    // 200 LEDs: ab dem 171. LED beginnt das nächste Universe
    Rig big;
    REQUIRE(big.player.loadText(R"(whitehole-project 1
destination 0 10.0.0.5
fixture Lang
  strip 200 from 0 0 0 to 200 0 0
  patch artnet destination 0 universe 0 channel 1
end
fixture Frei at 0 5 0
  matrix 2 2
end
node g wh.generator.gradient
end
output g.out
)").ok());
    const auto map = big.player.project()->layout.bake(big.player.project()->normalize);
    REQUIRE(map.ok());
    const auto last = runtime::describeLed(*big.player.project(), map.value(), 199, {});
    REQUIRE(last.has_value());
    REQUIRE(last->address.has_value());
    CHECK(last->address->universe == 1);
    CHECK(last->address->firstChannel == 1 + 29 * 3);
    const auto free = runtime::describeLed(*big.player.project(), map.value(), 200, {});
    REQUIRE(free.has_value());
    CHECK_FALSE(free->address.has_value());
    CHECK(runtime::formatLedInfo(*free).find("Nicht gepatcht") != std::string::npos);
}

TEST_CASE("LED-Info: Text nennt Fixture, Adresse und Farbe", "[ledinfo]") {
    Rig rig;
    const auto map = rig.player.project()->layout.bake(rig.player.project()->normalize);
    REQUIRE(map.ok());
    const std::vector<ColorRgba> colors(10, ColorRgba{1.0F, 0.5F, 0.0F, 1.0F});
    const auto info = runtime::describeLed(*rig.player.project(), map.value(), 0, colors);
    REQUIRE(info.has_value());
    const std::string text = runtime::formatLedInfo(*info);
    CHECK(text.find("Wand: LED 1 von 8") != std::string::npos);
    CHECK(text.find("Art-Net: Ziel 0, Universe 3, Kanal 10-12") != std::string::npos);
    CHECK(text.find("R 255  G 128  B 0") != std::string::npos);
    CHECK(text.back() != '\n');
}

TEST_CASE("Player: ledInfoAt ohne Lauf liefert die Adresse ohne Farbe", "[ledinfo]") {
    Rig rig;
    preview::PreviewOptions options;
    const auto image = rig.player.image(options);
    REQUIRE(image.ok());
    const auto map = rig.player.project()->layout.bake(rig.player.project()->normalize);
    const preview::PixelPoint p = preview::toPixel(image.value().frame, map.value().world[1]);
    const auto info = rig.player.ledInfoAt(image.value().frame, p.x, p.y, 3.0F);
    REQUIRE(info.has_value());
    CHECK(info->index == 1);
    CHECK_FALSE(info->color.has_value());
    CHECK(info->address.has_value());
    CHECK_FALSE(rig.player.ledInfoAt(image.value().frame, -500.0F, -500.0F, 3.0F).has_value());
}

TEST_CASE("Zuletzt geöffnet: neuester Eintrag vorn, keine Doppelten, höchstens zehn", "[recent]") {
    std::vector<std::string> list;
    list = runtime::addRecentFile(list, "/tmp/a.whfx");
    list = runtime::addRecentFile(list, "/tmp/b.whfx");
    list = runtime::addRecentFile(list, "/tmp/a.whfx");  // rutscht nach vorn
    REQUIRE(list.size() == 2);
    CHECK(list[0] == runtime::normalizedPath("/tmp/a.whfx"));
    CHECK(list[1] == runtime::normalizedPath("/tmp/b.whfx"));

    list = runtime::addRecentFile(list, "/tmp/x/../b.whfx");  // andere Schreibweise desselben Pfads
    REQUIRE(list.size() == 2);
    CHECK(list[0] == runtime::normalizedPath("/tmp/b.whfx"));

    CHECK(runtime::addRecentFile(list, "").size() == 2);  // leer: nichts
    for (int i = 0; i < 15; ++i) {
        list = runtime::addRecentFile(list, "/tmp/p" + std::to_string(i) + ".whfx");
    }
    CHECK(list.size() == runtime::kMaxRecentFiles);
    CHECK(list.front() == runtime::normalizedPath("/tmp/p14.whfx"));
    CHECK(runtime::addRecentFile(list, "/tmp/neu.whfx", 3).size() == 3);

    list = runtime::removeRecentFile(list, "/tmp/./p14.whfx");
    CHECK(list.front() == runtime::normalizedPath("/tmp/p13.whfx"));
    CHECK(runtime::removeRecentFile(list, "/tmp/gibtsnicht").size() == list.size());
}
