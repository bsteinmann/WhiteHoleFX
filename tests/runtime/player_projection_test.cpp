// Tests: Projektion (Volume/Faces/Box) im Player setzen, für einzelne Fixtures und ganze Gruppen, jeweils ein Undo-Schritt.
// Warum: Der Layout-Editor stellt eine Auswahl oder eine Gruppe in einem Zug um; der Nutzer muss das mit Strg+Z
// zurücknehmen können, und unbekannte Fixtures oder Gruppen müssen als Fehler ankommen statt still zu scheitern.
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;
using mapping::Projection;
using runtime::Player;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture A at 0 0 0
  matrix 4 2
  patch artnet destination 0 universe 0
end
fixture B at 10 3 0
  matrix 2 2
  patch artnet destination 0 universe 1
end
fixture C at 5 -4 0
  matrix 2 1
  patch artnet destination 0 universe 2
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

    [[nodiscard]] mapping::FixtureId id(std::size_t index) const {
        return player.project()->layout.fixtures().at(index).id;
    }
    [[nodiscard]] Projection projection(std::size_t index) const {
        return player.project()->layout.fixtures().at(index).projection;
    }
};

}  // namespace

TEST_CASE("Projektion: mehrere Fixtures umstellen ist ein Undo-Schritt", "[player][projection]") {
    Rig rig;
    REQUIRE(rig.player.setFixturesProjection({rig.id(0), rig.id(1)}, Projection::Box).ok());
    CHECK(rig.projection(0) == Projection::Box);
    CHECK(rig.projection(1) == Projection::Box);
    CHECK(rig.projection(2) == Projection::Volume);

    REQUIRE(rig.player.undo().ok());
    CHECK(rig.projection(0) == Projection::Volume);
    CHECK(rig.projection(1) == Projection::Volume);
}

TEST_CASE("Projektion: Gruppe setzt alle ihre Fixtures auf einmal, ein Undo-Schritt", "[player][projection]") {
    Rig rig;
    REQUIRE(rig.player.setGroups({project::FixtureGroup{"Wand", {"A", "B"}}}).ok());
    REQUIRE(rig.player.setGroupProjection("Wand", Projection::Faces).ok());
    CHECK(rig.projection(0) == Projection::Faces);
    CHECK(rig.projection(1) == Projection::Faces);
    CHECK(rig.projection(2) == Projection::Volume);  // C gehört nicht zur Gruppe

    REQUIRE(rig.player.undo().ok());  // nur die Umstellung, die Gruppe bleibt
    CHECK(rig.projection(0) == Projection::Volume);
    CHECK(rig.player.project()->groups.size() == 1);
}

TEST_CASE("Projektion: Fehler bei leerer Auswahl, unbekannter Id, Gruppe oder Fixture", "[player][projection]") {
    Rig rig;
    CHECK(rig.player.setFixturesProjection({}, Projection::Box).error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.setFixturesProjection({mapping::FixtureId{99}}, Projection::Box).error().code ==
          ErrorCode::NotFound);
    CHECK(rig.player.setGroupProjection("Gibt-es-nicht", Projection::Box).error().code == ErrorCode::NotFound);

    REQUIRE(rig.player.setGroups({project::FixtureGroup{"Geist", {"A", "Nirgends"}}}).ok());
    CHECK(rig.player.setGroupProjection("Geist", Projection::Box).error().code == ErrorCode::NotFound);
    // Der Fehler lässt das Layout unverändert: auch das bekannte Fixture A bleibt bei Volume.
    CHECK(rig.projection(0) == Projection::Volume);
}

TEST_CASE("Projektion: ohne Projekt meldet der Player einen Fehler", "[player][projection]") {
    auto library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    Player player{&device, *library, gate};
    CHECK_FALSE(player.setGroupProjection("Wand", Projection::Box).ok());
}
