// Tests: Testbilder (reine Rechnung), ihre Einhängung in die Session und die Player-Schnittstelle.
// Das FakeGpuDevice liefert Nullen; was hier an Farben ankommt, stammt also vom Testbild.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"
#include "whitehole/runtime/test_pattern.hpp"

using namespace whitehole;
using runtime::Player;
using runtime::SweepAxis;
using runtime::TestPattern;
using runtime::TestPatternKind;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

// A: 4x2 = 8 LEDs (links), B: 2x1 = 2 LEDs (rechts oben).
const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture A at 0 0 0
  matrix 4 2
  patch artnet destination 0 universe 0
end
fixture B at 8 0 0
  matrix 2 1
  patch artnet destination 0 universe 1
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
    Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }
    [[nodiscard]] mapping::FixtureId id(std::size_t i) const { return player.project()->layout.fixtures().at(i).id; }
};

mapping::LedMap mapOf(Rig& rig) {
    auto baked = rig.player.project()->layout.bake(rig.player.project()->normalize);
    REQUIRE(baked.ok());
    return std::move(baked).value();
}

bool isOff(const ColorRgba& c) { return c.r == 0.0F && c.g == 0.0F && c.b == 0.0F; }

TestPattern pattern(TestPatternKind kind) {
    TestPattern p;
    p.kind = kind;
    return p;
}

}  // namespace

TEST_CASE("Testbild: Aus und Farbe", "[testpattern]") {
    Rig rig;
    const mapping::LedMap map = mapOf(rig);
    for (const ColorRgba& c : runtime::renderTestPattern(pattern(TestPatternKind::Off), map, 1.0)) {
        CHECK(isOff(c));
    }
    TestPattern p = pattern(TestPatternKind::Color);
    p.color = ColorRgba{0.0F, 0.5F, 1.0F, 1.0F};
    const auto colors = runtime::renderTestPattern(p, map, 0.0);
    REQUIRE(colors.size() == 10);
    for (const ColorRgba& c : colors) {
        CHECK(c == p.color);
    }
    // Auf ein Fixture beschränkt: B bleibt aus.
    p.fixtures = {rig.id(0)};
    const auto only = runtime::renderTestPattern(p, map, 0.0);
    CHECK(only[7] == p.color);
    CHECK(isOff(only[8]));
    CHECK(isOff(only[9]));
}

TEST_CASE("Testbild: Auswahl hervorheben leuchtet nur auf den gewählten Fixtures, leer heißt nichts", "[testpattern]") {
    Rig rig;
    const mapping::LedMap map = mapOf(rig);
    TestPattern p = pattern(TestPatternKind::Highlight);
    for (const ColorRgba& c : runtime::renderTestPattern(p, map, 0.0)) {
        CHECK(isOff(c));
    }
    p.fixtures = {rig.id(1)};
    const auto colors = runtime::renderTestPattern(p, map, 0.0);
    for (std::size_t i = 0; i < 8; ++i) {
        CHECK(isOff(colors[i]));
    }
    CHECK(colors[8] == ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    CHECK(colors[9] == ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
}

TEST_CASE("Testbild: Laufbalken wandert über die Achse", "[testpattern]") {
    Rig rig;
    const mapping::LedMap map = mapOf(rig);
    TestPattern p = pattern(TestPatternKind::Sweep);
    p.rate = 1.0F;  // ein Durchlauf pro Sekunde
    const auto litAt = [&](double t) {
        std::vector<float> xs;
        const auto colors = runtime::renderTestPattern(p, map, t);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            if (!isOff(colors[i])) {
                xs.push_back(map.unit[i].x);
            }
        }
        return xs;
    };
    const auto early = litAt(0.1);
    const auto late = litAt(0.9);
    REQUIRE_FALSE(early.empty());
    REQUIRE_FALSE(late.empty());
    for (float x : early) {
        CHECK(x < 0.3F);
    }
    for (float x : late) {
        CHECK(x > 0.7F);
    }
    // Nach einem vollen Durchlauf wieder am Anfang.
    CHECK(litAt(1.1) == early);
    // Die y-Achse: bei t=0 unten (y klein).
    // Die y-Achse: Der Balken erfasst im Lauf jede LED, und zu Beginn nur die unteren.
    p.axis = SweepAxis::Y;
    std::vector<bool> seen(map.size(), false);
    for (double t = 0.0; t < 1.0; t += 0.02) {
        const auto colors = runtime::renderTestPattern(p, map, t);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            if (!isOff(colors[i])) {
                seen[i] = true;
                if (t < 0.1) {
                    CHECK(map.unit[i].y < 0.3F);
                }
            }
        }
    }
    CHECK(std::count(seen.begin(), seen.end(), false) == 0);
}

TEST_CASE("Testbild: Farbreihenfolge zeigt Rot, Grün, Blau, Weiß", "[testpattern]") {
    Rig rig;
    const mapping::LedMap map = mapOf(rig);
    TestPattern p = pattern(TestPatternKind::ColorOrder);
    p.rate = 1.0F;
    CHECK(runtime::renderTestPattern(p, map, 0.5)[3] == ColorRgba{1, 0, 0, 1});
    CHECK(runtime::renderTestPattern(p, map, 1.5)[3] == ColorRgba{0, 1, 0, 1});
    CHECK(runtime::renderTestPattern(p, map, 2.5)[3] == ColorRgba{0, 0, 1, 1});
    CHECK(runtime::renderTestPattern(p, map, 3.5)[3] == ColorRgba{1, 1, 1, 1});
    CHECK(runtime::renderTestPattern(p, map, 4.5)[3] == ColorRgba{1, 0, 0, 1});
}

TEST_CASE("Testbild: Lauflicht läuft je Fixture in Kabelreihenfolge", "[testpattern]") {
    Rig rig;
    const mapping::LedMap map = mapOf(rig);
    TestPattern p = pattern(TestPatternKind::Chase);
    p.rate = 1.0F;  // eine LED pro Sekunde
    auto colors = runtime::renderTestPattern(p, map, 0.0);
    CHECK(colors[0].r == 1.0F);  // Kopf auf der ersten LED von A
    CHECK(colors[8].r == 1.0F);  // und auf der ersten LED von B
    CHECK(colors[1].r == 0.0F);
    colors = runtime::renderTestPattern(p, map, 2.0);
    CHECK(colors[2].r == 1.0F);
    CHECK(colors[1].r > 0.0F);   // Schweif hinter dem Kopf
    CHECK(colors[1].r < 1.0F);
    CHECK(colors[3].r == 0.0F);  // nichts vor dem Kopf
    // B hat zwei LEDs: nach zwei Schritten wieder vorn.
    CHECK(colors[8].r == 1.0F);
}

TEST_CASE("Testbild: ungültige Zeit und ungültiges Tempo fallen auf Standardwerte", "[testpattern]") {
    Rig rig;
    const mapping::LedMap map = mapOf(rig);
    TestPattern p = pattern(TestPatternKind::Sweep);
    p.rate = std::numeric_limits<float>::quiet_NaN();
    CHECK(runtime::renderTestPattern(p, map, std::nan("")).size() == 10);
    p.rate = -3.0F;
    CHECK(runtime::renderTestPattern(p, map, -5.0).size() == 10);
    CHECK(runtime::defaultTestRate(TestPatternKind::Chase) > 0.0F);
    CHECK(std::string{runtime::testPatternName(TestPatternKind::Sweep)} == "Laufbalken");
}

TEST_CASE("Player: Testbild ersetzt den Effekt in Vorschau und Ausgabe, ohne das Projekt zu ändern", "[testpattern][player]") {
    Rig rig;
    CHECK(rig.player.testPattern() == nullptr);
    REQUIRE(rig.player.start().ok());
    rig.gate.setEnabled(true);

    TestPattern p = pattern(TestPatternKind::Color);
    p.color = ColorRgba{1.0F, 0.0F, 0.0F, 1.0F};
    REQUIRE(rig.player.setTestPattern(p).ok());
    REQUIRE(rig.player.testPattern() != nullptr);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (rig.recording.count() < 4 && std::chrono::steady_clock::now() < deadline) {
        REQUIRE(rig.player.step().ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    CHECK_FALSE(rig.player.modified());
    CHECK_FALSE(rig.player.canUndo());
    const auto image = rig.player.image({});
    REQUIRE(image.ok());
    std::size_t red = 0;
    for (std::size_t i = 0; i < image.value().rgba.size(); i += 4) {
        if (image.value().rgba[i] > 100 && image.value().rgba[i + 1] < 50 && image.value().rgba[i + 2] < 50) {
            ++red;
        }
    }
    CHECK(red > 50);
    // Ins Netz gingen Pakete mit Daten: Das Fake rechnet Nullen, nur das Testbild macht Rot daraus.
    const auto packets = rig.recording.packets();
    REQUIRE_FALSE(packets.empty());
    bool nonZero = false;
    for (const auto& packet : packets) {
        for (std::size_t i = 18; i < packet.bytes.size(); ++i) {
            nonZero = nonZero || packet.bytes[i] != 0;
        }
    }
    CHECK(nonZero);

    rig.player.clearTestPattern();
    CHECK(rig.player.testPattern() == nullptr);
}

TEST_CASE("Player: Testbild überlebt Neustart der Session, nicht aber ein neues Projekt", "[testpattern][player]") {
    Rig rig;
    TestPattern p = pattern(TestPatternKind::Highlight);
    p.fixtures = {rig.id(0)};
    REQUIRE(rig.player.setTestPattern(p).ok());  // noch ohne Lauf: wird gemerkt
    REQUIRE(rig.player.start().ok());
    CHECK(rig.player.testPattern() != nullptr);
    rig.player.stop();
    REQUIRE(rig.player.start().ok());
    CHECK(rig.player.testPattern() != nullptr);

    REQUIRE(rig.player.loadText(kProject).ok());
    CHECK(rig.player.testPattern() == nullptr);
}

TEST_CASE("Player: Testbild braucht ein Projekt und eine gültige Farbe", "[testpattern][player]") {
    auto library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    Player empty{&device, *library, gate};
    CHECK_FALSE(empty.setTestPattern(pattern(TestPatternKind::Color)).ok());

    Rig rig;
    TestPattern bad = pattern(TestPatternKind::Color);
    bad.color.g = std::numeric_limits<float>::infinity();
    CHECK_FALSE(rig.player.setTestPattern(bad).ok());
    bad.color = ColorRgba{2.0F, -1.0F, 0.5F, 0.0F};
    REQUIRE(rig.player.setTestPattern(bad).ok());
    const TestPattern* stored = rig.player.testPattern();
    REQUIRE(stored != nullptr);
    CHECK(stored->color == ColorRgba{1.0F, 0.0F, 0.5F, 1.0F});
}
