// Tests: Fixture-Bibliothek (Bauformen) und Drehung im Player, ihre Datei-Schreibweise und Rückgängig.
#include <catch2/catch_test_macros.hpp>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;
using runtime::Player;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Basis at 100 100 0
  matrix 1 1
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

    [[nodiscard]] const mapping::Fixture* fixture(const std::string& name) const {
        for (const mapping::Fixture& f : player.project()->layout.fixtures()) {
            if (f.name == name) {
                return &f;
            }
        }
        return nullptr;
    }
};

mapping::MatrixShape matrix(std::uint32_t columns, std::uint32_t rows) {
    mapping::MatrixShape m;
    m.columns = columns;
    m.rows = rows;
    return m;
}

}  // namespace

TEST_CASE("Bibliothek: Bauform anlegen, doppelt und ungültig", "[player][types]") {
    Rig rig;
    REQUIRE(rig.player.addFixtureType("Matrix 4x2", matrix(4, 2)).ok());
    REQUIRE(rig.player.fixtureTypes().size() == 1);
    CHECK(rig.player.addFixtureType("Matrix 4x2", matrix(2, 2)).error().code == ErrorCode::AlreadyExists);
    CHECK(rig.player.addFixtureType("", matrix(2, 2)).error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.addFixtureType("Null", matrix(0, 2)).error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.fixtureTypes().size() == 1);
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.fixtureTypes().empty());
}

TEST_CASE("Bibliothek: Exemplare folgen der Bauform", "[player][types]") {
    Rig rig;
    REQUIRE(rig.player.addFixtureType("Kachel", matrix(2, 2)).ok());
    REQUIRE(rig.player.addFixtureOfType("A", "Kachel", Vec3{}, Vec3{}).ok());
    REQUIRE(rig.player.addFixtureOfType("B", "Kachel", Vec3{10.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 90.0F}).ok());
    CHECK(rig.fixture("B")->rotation == Vec3{0.0F, 0.0F, 90.0F});
    CHECK(rig.fixture("A")->type == "Kachel");

    REQUIRE(rig.player.replaceFixtureType("Kachel", matrix(3, 3)).ok());
    for (const char* name : {"A", "B"}) {
        const auto* shape = std::get_if<mapping::MatrixShape>(&rig.fixture(name)->shape);
        REQUIRE(shape != nullptr);
        CHECK(shape->columns == 3);
    }
    CHECK(rig.fixture("B")->rotation == Vec3{0.0F, 0.0F, 90.0F});  // Lage und Drehung bleiben
    REQUIRE(rig.player.undo().ok());
    CHECK(std::get<mapping::MatrixShape>(rig.fixture("A")->shape).columns == 2);
}

TEST_CASE("Bibliothek: umbenennen, entfernen und lösen", "[player][types]") {
    Rig rig;
    REQUIRE(rig.player.addFixtureType("Kachel", matrix(2, 2)).ok());
    REQUIRE(rig.player.addFixtureOfType("A", "Kachel", Vec3{}, Vec3{}).ok());
    REQUIRE(rig.player.addFixtureType("Zweite", matrix(1, 1)).ok());
    CHECK(rig.player.renameFixtureType("Kachel", "Zweite").error().code == ErrorCode::AlreadyExists);
    CHECK(rig.player.renameFixtureType("Gibtsnicht", "X").error().code == ErrorCode::NotFound);
    REQUIRE(rig.player.renameFixtureType("Kachel", "Platte").ok());
    CHECK(rig.fixture("A")->type == "Platte");

    REQUIRE(rig.player.removeFixtureType("Platte").ok());
    CHECK(rig.fixture("A")->type.empty());
    CHECK(std::get<mapping::MatrixShape>(rig.fixture("A")->shape).columns == 2);  // behält die Form
    CHECK(rig.player.removeFixtureType("Platte").error().code == ErrorCode::NotFound);

    REQUIRE(rig.player.assignFixtureType(rig.fixture("A")->id, "Zweite").ok());
    CHECK(std::get<mapping::MatrixShape>(rig.fixture("A")->shape).columns == 1);
    REQUIRE(rig.player.assignFixtureType(rig.fixture("A")->id, "").ok());
    CHECK(rig.fixture("A")->type.empty());
    CHECK(rig.player.assignFixtureType(rig.fixture("A")->id, "Nix").error().code == ErrorCode::NotFound);
}

TEST_CASE("Bibliothek: unbekannte Bauform, doppelter Fixture-Name", "[player][types]") {
    Rig rig;
    CHECK(rig.player.addFixtureOfType("A", "Nix", Vec3{}, Vec3{}).error().code == ErrorCode::NotFound);
    REQUIRE(rig.player.addFixtureType("Kachel", matrix(2, 2)).ok());
    REQUIRE(rig.player.addFixtureOfType("A", "Kachel", Vec3{}, Vec3{}).ok());
    CHECK_FALSE(rig.player.addFixtureOfType("A", "Kachel", Vec3{}, Vec3{}).ok());
    CHECK(rig.player.project()->layout.fixtures().size() == 2);  // Basis und A
}

TEST_CASE("Drehung: setzen, Undo und Datei-Rundreise", "[player][types]") {
    Rig rig;
    REQUIRE(rig.player.addFixtureType("Ring", mapping::RingShape{}).ok());
    REQUIRE(rig.player.addFixtureType("Säule", mapping::HelixShape{}).ok());
    REQUIRE(rig.player.addFixtureOfType("R1", "Ring", Vec3{1.0F, 2.0F, 3.0F}, Vec3{10.0F, 20.0F, 30.0F}).ok());
    REQUIRE(rig.player.addFixtureOfType("S1", "Säule", Vec3{}, Vec3{}).ok());
    const auto id = rig.fixture("R1")->id;
    REQUIRE(rig.player.setFixtureRotation(id, Vec3{0.0F, 45.0F, 0.0F}).ok());
    CHECK(rig.fixture("R1")->rotation == Vec3{0.0F, 45.0F, 0.0F});
    CHECK(rig.player.setFixtureRotation(mapping::FixtureId{999}, Vec3{}).error().code == ErrorCode::NotFound);
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.fixture("R1")->rotation == Vec3{10.0F, 20.0F, 30.0F});

    const auto text = project::writeProject(*rig.player.project());
    REQUIRE(text.ok());
    CHECK(text.value().find("fixturetype") != std::string::npos);
    CHECK(text.value().find("rotate") != std::string::npos);
    CHECK(text.value().find("ring") != std::string::npos);
    CHECK(text.value().find("helix") != std::string::npos);
    Rig other;
    REQUIRE(other.player.loadText(text.value()).ok());
    REQUIRE(other.fixture("R1") != nullptr);
    CHECK(other.fixture("R1")->rotation == Vec3{10.0F, 20.0F, 30.0F});
    CHECK(other.fixture("R1")->type == "Ring");
    CHECK(other.player.fixtureTypes().size() == 2);
    const auto* helix = std::get_if<mapping::HelixShape>(&other.fixture("S1")->shape);
    REQUIRE(helix != nullptr);
    CHECK(helix->count == mapping::HelixShape{}.count);
    CHECK(helix->turns == mapping::HelixShape{}.turns);
}

TEST_CASE("Datei: Bauform ohne Definition und alte Dateien", "[player][types]") {
    Rig rig;
    const std::string bad = std::string{kProject} + "fixture A at 0 0 0 type \"Fehlt\"\n  patch artnet destination 0 universe 2\nend\n";
    const auto loaded = rig.player.loadText(bad);
    REQUIRE_FALSE(loaded.ok());
    CHECK(loaded.error().message.find("Fehlt") != std::string::npos);
    // Eine alte Datei ohne Bibliothek bleibt unverändert lesbar.
    const std::string old = "whitehole-project 1\ndestination 0 10.0.0.5\nfixture A at 0 0 0\n  matrix 2 2\n  patch artnet destination 0 universe 1\nend\nnode g wh.generator.gradient\nend\noutput g.out\n";
    REQUIRE(rig.player.loadText(old).ok());
    CHECK(rig.player.fixtureTypes().empty());
    CHECK(rig.fixture("A")->rotation == Vec3{});
}

TEST_CASE("Ziele: ein mittleres Ziel entfernen, höhere Patches rücken nach", "[player][destinations]") {
    Rig rig;
    REQUIRE(rig.player.setDestinations({output::Destination{"10.0.0.5", 0}, output::Destination{"10.0.0.6", 0},
                                        output::Destination{"10.0.0.7", 0}}).ok());
    const auto basis = rig.fixture("Basis")->id;
    // Basis sendet auf Ziel 0; zwei weitere Fixtures auf 1 und 2
    REQUIRE(rig.player.addFixtureType("Kachel", matrix(2, 2)).ok());
    output::FixturePatch one;
    one.destination = 1;
    one.universe = 0;
    output::FixturePatch two;
    two.destination = 2;
    two.universe = 0;
    REQUIRE(rig.player.addFixtureOfType("B", "Kachel", Vec3{10.0F, 0.0F, 0.0F}, Vec3{}, one).ok());
    REQUIRE(rig.player.addFixtureOfType("C", "Kachel", Vec3{20.0F, 0.0F, 0.0F}, Vec3{}, two).ok());

    const auto before = rig.player.project()->destinations.size();
    // Ziel 1 wird noch von B benutzt: nichts ändert sich
    const auto refused = rig.player.removeDestination(1);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("B") != std::string::npos);
    CHECK(rig.player.project()->destinations.size() == before);

    // B auf Ziel 0 umpatchen (anderes Universe), dann Ziel 1 entfernen: C rückt von 2 auf 1
    one.destination = 0;
    one.universe = 5;
    REQUIRE(rig.player.setPatch(rig.fixture("B")->id, one).ok());
    REQUIRE(rig.player.removeDestination(1).ok());
    CHECK(rig.player.project()->destinations.size() == before - 1);
    CHECK(rig.player.project()->destinations[1].address == "10.0.0.7");
    for (const output::FixturePatch& patch : rig.player.project()->patches) {
        if (patch.fixture == rig.fixture("C")->id) {
            CHECK(patch.destination == 1);
        }
        if (patch.fixture == basis) {
            CHECK(patch.destination == 0);
        }
    }
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->destinations.size() == before);
    CHECK(rig.player.removeDestination(99).error().code == ErrorCode::NotFound);
}
