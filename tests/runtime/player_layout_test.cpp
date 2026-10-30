// Tests der Layout-, Ausgabe- und Einstellungs-Bearbeitung im Player (Grundlage des Layout-Editors).
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
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
node g wh.generator.gradient
end
output g.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    [[nodiscard]] mapping::FixtureId panel() const { return player.project()->layout.fixtures().front().id; }
    [[nodiscard]] std::size_t fixtures() const { return player.project()->layout.fixtures().size(); }
    [[nodiscard]] std::uint32_t leds() const { return player.status().ledCount; }
};

output::FixturePatch patchOn(std::uint16_t universe, std::uint32_t destination = 0) {
    output::FixturePatch p;
    p.destination = destination;
    p.universe = universe;
    return p;
}

mapping::StripShape strip(std::uint32_t count) {
    mapping::StripShape s;
    s.count = count;
    s.end = Vec3{static_cast<float>(count), 0.0F, 0.0F};
    return s;
}

}  // namespace

TEST_CASE("Layout: Fixture mit Patch hinzufügen", "[player][layout]") {
    Rig rig;
    REQUIRE(rig.leds() == 8);
    const auto id = rig.player.addFixture("Decke", Vec3{0.0F, 5.0F, 0.0F}, strip(30), patchOn(2));
    REQUIRE(id.ok());
    CHECK(rig.fixtures() == 2);
    CHECK(rig.leds() == 38);
    CHECK(rig.player.project()->patches.size() == 2);
    CHECK(rig.player.project()->patches.back().fixture == id.value());
    CHECK(rig.player.modified());
    CHECK(rig.player.image({}).ok());
}

TEST_CASE("Layout: abgelehnte Änderungen lassen alles unverändert", "[player][layout]") {
    Rig rig;
    CHECK(rig.player.addFixture("Panel", Vec3{}, strip(3), patchOn(2)).error().code == ErrorCode::InvalidArgument);  // Name doppelt
    CHECK_FALSE(rig.player.addFixture("", Vec3{}, strip(3), patchOn(2)).ok());                                      // Name leer
    CHECK_FALSE(rig.player.addFixture("Null", Vec3{}, strip(0), patchOn(2)).ok());                                  // Form ungültig
    CHECK_FALSE(rig.player.addFixture("Kollision", Vec3{}, strip(3), patchOn(1)).ok());                             // Kanäle belegt
    CHECK_FALSE(rig.player.addFixture("Ziel", Vec3{}, strip(3), patchOn(2, 7)).ok());                               // Ziel 7 gibt es nicht
    CHECK(rig.fixtures() == 1);
    CHECK(rig.leds() == 8);
    CHECK(rig.player.project()->patches.size() == 1);
    CHECK_FALSE(rig.player.canUndo());
    CHECK_FALSE(rig.player.modified());
}

TEST_CASE("Layout: Fixture entfernen löscht seinen Patch; das letzte gepatchte bleibt", "[player][layout]") {
    Rig rig;
    const auto deck = rig.player.addFixture("Decke", Vec3{}, strip(30), patchOn(2));
    REQUIRE(deck.ok());
    REQUIRE(rig.player.removeFixture(deck.value()).ok());
    CHECK(rig.fixtures() == 1);
    CHECK(rig.player.project()->patches.size() == 1);

    const auto r = rig.player.removeFixture(rig.panel());  // wäre das einzige gepatchte
    REQUIRE_FALSE(r.ok());
    CHECK(rig.fixtures() == 1);
    CHECK(rig.player.removeFixture(mapping::FixtureId{99}).error().code == ErrorCode::NotFound);
}

TEST_CASE("Layout: umbenennen, verschieben, Form ändern", "[player][layout]") {
    Rig rig;
    const auto id = rig.panel();
    REQUIRE(rig.player.renameFixture(id, "Wand").ok());
    CHECK(rig.player.project()->layout.find(id)->name == "Wand");
    REQUIRE(rig.player.moveFixture(id, Vec3{1.0F, 2.0F, 3.0F}).ok());
    CHECK(rig.player.project()->layout.find(id)->origin == Vec3{1.0F, 2.0F, 3.0F});

    mapping::MatrixShape big;
    big.columns = 16;
    big.rows = 8;
    REQUIRE(rig.player.setFixtureShape(id, big).ok());
    CHECK(rig.leds() == 128);

    mapping::MatrixShape huge;  // 25 Mio. LEDs übersteigen die Obergrenze je Fixture
    huge.columns = 5000;
    huge.rows = 5000;
    CHECK_FALSE(rig.player.setFixtureShape(id, huge).ok());
    CHECK(rig.leds() == 128);

    CHECK_FALSE(rig.player.setFixtureShape(id, strip(0)).ok());
    CHECK(rig.player.renameFixture(id, "").error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.renameFixture(mapping::FixtureId{99}, "x").error().code == ErrorCode::NotFound);
    CHECK(rig.player.moveFixture(id, Vec3{std::nanf(""), 0.0F, 0.0F}).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("Layout: Patch setzen und entfernen", "[player][layout]") {
    Rig rig;
    const auto deck = rig.player.addFixture("Decke", Vec3{}, strip(30));  // ohne Patch
    REQUIRE(deck.ok());
    CHECK(rig.player.project()->patches.size() == 1);
    REQUIRE(rig.player.setPatch(deck.value(), patchOn(2)).ok());
    CHECK(rig.player.project()->patches.size() == 2);
    auto changed = patchOn(3);
    changed.color.order = output::ColorOrder::GRB;
    REQUIRE(rig.player.setPatch(deck.value(), changed).ok());
    CHECK(rig.player.project()->patches.size() == 2);
    CHECK(rig.player.project()->patches.back().universe == 3);
    CHECK(rig.player.project()->patches.back().color.order == output::ColorOrder::GRB);
    REQUIRE(rig.player.setPatch(deck.value(), std::nullopt).ok());
    CHECK(rig.player.project()->patches.size() == 1);
    CHECK_FALSE(rig.player.setPatch(rig.panel(), std::nullopt).ok());  // der einzige Patch
    CHECK(rig.player.setPatch(mapping::FixtureId{99}, patchOn(5)).error().code == ErrorCode::NotFound);
}

TEST_CASE("Layout: Ziele ersetzen", "[player][layout]") {
    Rig rig;
    CHECK_FALSE(rig.player.setDestinations({{"keine-adresse", 0}}).ok());
    CHECK_FALSE(rig.player.setDestinations({}).ok());  // Patch zeigt auf Ziel 0
    CHECK(rig.player.project()->destinations.size() == 1);

    REQUIRE(rig.player.setDestinations({{"10.0.0.5", 0}, {"10.0.0.6", 6454}}).ok());
    CHECK(rig.player.project()->destinations.size() == 2);
    CHECK(rig.player.addFixture("Decke", Vec3{}, strip(30), patchOn(2, 1)).ok());
    CHECK_FALSE(rig.player.setDestinations({{"10.0.0.5", 0}}).ok());  // Ziel 1 wird noch gebraucht
}

TEST_CASE("Layout: suggestPatch wählt das erste freie Universe", "[player][layout]") {
    Rig rig;
    // Panel 4x2 = 8 LEDs auf Universe 1 -> belegt Universe 1, frei ab 2
    CHECK(rig.player.suggestPatch(30).universe == 2);
    const auto deck = rig.player.addFixture("Decke", Vec3{}, strip(400), rig.player.suggestPatch(400));
    REQUIRE(deck.ok());  // Universe 2, 400 LEDs = 3 Universes (2, 3, 4)
    CHECK(rig.player.suggestPatch(10).universe == 5);
    CHECK(rig.player.addFixture("Noch ein Strip", Vec3{}, strip(10), rig.player.suggestPatch(10)).ok());
}

TEST_CASE("Layout: Einstellungen", "[player][layout]") {
    Rig rig;
    runtime::Player::Settings s;
    s.framesPerSecond = 25.0;
    s.normalize = mapping::NormalizeMode::KeepAspect;
    s.sacnPriority = 150;
    s.sourceName = "Bühne";
    REQUIRE(rig.player.setSettings(s).ok());
    CHECK(rig.player.framesPerSecond() == 25.0);
    CHECK(rig.player.project()->normalize == mapping::NormalizeMode::KeepAspect);
    CHECK(rig.player.project()->sacnPriority == 150);
    CHECK(rig.player.project()->sourceName == "Bühne");

    auto bad = s;
    bad.framesPerSecond = 0.0;
    CHECK_FALSE(rig.player.setSettings(bad).ok());
    bad = s;
    bad.framesPerSecond = 1001.0;
    CHECK_FALSE(rig.player.setSettings(bad).ok());
    bad = s;
    bad.sacnPriority = 201;
    CHECK_FALSE(rig.player.setSettings(bad).ok());
    bad = s;
    bad.sourceName.clear();
    CHECK_FALSE(rig.player.setSettings(bad).ok());
    CHECK(rig.player.framesPerSecond() == 25.0);
}

TEST_CASE("Layout: Rückgängig stellt Layout, Patches und Vorschau wieder her", "[player][layout][undo]") {
    Rig rig;
    REQUIRE(rig.player.addFixture("Decke", Vec3{}, strip(30), patchOn(2)).ok());
    REQUIRE(rig.leds() == 38);
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.fixtures() == 1);
    CHECK(rig.leds() == 8);
    CHECK(rig.player.project()->patches.size() == 1);
    REQUIRE(rig.player.redo().ok());
    CHECK(rig.fixtures() == 2);
    CHECK(rig.leds() == 38);
}

TEST_CASE("Layout: Änderungen im Lauf bauen die Session neu auf", "[player][layout]") {
    Rig rig;
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());
    const std::size_t pipelines = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.addFixture("Decke", Vec3{}, strip(30), patchOn(2)).ok());
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.status().running);
    CHECK(rig.player.status().ledCount == 38);
    CHECK_FALSE(rig.player.status().lastError.has_value());
    CHECK(rig.device.pipelineDescs().size() > pipelines);

    const std::size_t afterAdd = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.undo().ok());
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.status().running);
    CHECK(rig.device.pipelineDescs().size() > afterAdd);
}

TEST_CASE("Layout: Verschieben und Umbenennen fassen gleichartige Schritte zusammen", "[player][layout][undo]") {
    Rig rig;
    for (int i = 1; i <= 10; ++i) {
        REQUIRE(rig.player.moveFixture(rig.panel(), Vec3{static_cast<float>(i), 0.0F, 0.0F}).ok());
    }
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->layout.find(rig.panel())->origin == Vec3{});
    CHECK_FALSE(rig.player.canUndo());
}

TEST_CASE("Layout: Speichern und Laden bewahrt Fixtures, Ziele und Einstellungen", "[player][layout]") {
    Rig rig;
    REQUIRE(rig.player.addFixture("Decke", Vec3{0.0F, 5.0F, 0.0F}, strip(30), patchOn(2)).ok());
    REQUIRE(rig.player.setDestinations({{"10.0.0.5", 0}, {"10.0.0.9", 6454}}).ok());
    runtime::Player::Settings s;
    s.framesPerSecond = 30.0;
    s.sourceName = "Probe";
    REQUIRE(rig.player.setSettings(s).ok());

    const std::string path = (std::filesystem::temp_directory_path() / "whfx_player_layout_test.whfx").string();
    REQUIRE(rig.player.save(path).ok());
    runtime::Player other{&rig.device, *rig.library, rig.gate};
    REQUIRE(other.load(path).ok());
    std::filesystem::remove(path);
    CHECK(other.project()->layout.fixtures().size() == 2);
    CHECK(other.project()->destinations.size() == 2);
    CHECK(other.project()->destinations[1].port == 6454);
    CHECK(other.framesPerSecond() == 30.0);
    CHECK(other.project()->sourceName == "Probe");
    CHECK(other.status().ledCount == 38);
}

TEST_CASE("Layout: Bearbeiten ohne Projekt ist ein Fehler", "[player][layout]") {
    auto library = nodes::BuiltinLibrary::create().value();
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{nullptr, *library, gate};
    CHECK_FALSE(player.addFixture("x", Vec3{}, strip(3)).ok());
    CHECK_FALSE(player.removeFixture(mapping::FixtureId{1}).ok());
    CHECK_FALSE(player.renameFixture(mapping::FixtureId{1}, "y").ok());
    CHECK_FALSE(player.moveFixture(mapping::FixtureId{1}, Vec3{}).ok());
    CHECK_FALSE(player.setFixtureShape(mapping::FixtureId{1}, strip(3)).ok());
    CHECK_FALSE(player.setPatch(mapping::FixtureId{1}, std::nullopt).ok());
    CHECK_FALSE(player.setDestinations({}).ok());
    CHECK_FALSE(player.setSettings({}).ok());
}
