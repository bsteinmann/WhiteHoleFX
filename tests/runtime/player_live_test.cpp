// Look-Liste und Live-Betrieb im Player (mit Attrappen-Gerät, ohne GPU).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
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
node spiegel wh.transform.mirror
end
connect plasma.out -> spiegel.source
output spiegel.out

look Ruhig fade 3
node n1 wh.generator.plasma
  set scale 2
end
node n2 wh.transform.mirror
end
connect n1.out -> n2.source
output n2.out
endlook

look Schnell fade 0.5
node a wh.generator.noise
end
output a.out
endlook
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    double clock{0.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
    }
};

}  // namespace

TEST_CASE("Player/Looks: Liste aus der Datei, Speichern, Ersetzen", "[player][live]") {
    Rig rig;
    REQUIRE(rig.player.looks().size() == 2);
    CHECK(rig.player.looks()[0].name == "Ruhig");

    const auto saved = rig.player.saveLook("Neu", 1.5);
    REQUIRE(saved.ok());
    CHECK(saved.value() == 2);
    CHECK(rig.player.looks()[2].graph.nodes().size() == 2);  // Plasma + Mirror aus dem Editor-Graphen
    CHECK(rig.player.looks()[2].fadeSeconds == Approx(1.5));
    CHECK(rig.player.modified());

    // Gleicher Name ersetzt und behält Platz und Zeit, wenn keine neue Zeit kommt
    REQUIRE(rig.player.removeNode(rig.player.project()->graph.nodes().back().id).ok());
    const auto again = rig.player.saveLook("Neu");
    REQUIRE(again.ok());
    CHECK(again.value() == 2);
    CHECK(rig.player.looks().size() == 3);
    CHECK(rig.player.looks()[2].fadeSeconds == Approx(1.5));

    CHECK_FALSE(rig.player.saveLook("").ok());
    CHECK_FALSE(rig.player.saveLook("x", -1.0).ok());
}

TEST_CASE("Player/Looks: umbenennen, Zeit, verschieben, löschen", "[player][live]") {
    Rig rig;
    CHECK(rig.player.renameLook(0, "Sanft").ok());
    CHECK(rig.player.looks()[0].name == "Sanft");
    CHECK(rig.player.renameLook(0, "Schnell").error().code == ErrorCode::AlreadyExists);
    CHECK_FALSE(rig.player.renameLook(0, "").ok());
    CHECK(rig.player.renameLook(5, "x").error().code == ErrorCode::NotFound);

    CHECK(rig.player.setLookFade(1, 4.0).ok());
    CHECK(rig.player.looks()[1].fadeSeconds == Approx(4.0));
    CHECK_FALSE(rig.player.setLookFade(1, 601.0).ok());

    REQUIRE(rig.player.moveLook(0, 1).ok());
    CHECK(rig.player.looks()[0].name == "Schnell");
    CHECK(rig.player.looks()[1].name == "Sanft");
    CHECK_FALSE(rig.player.moveLook(0, 7).ok());

    REQUIRE(rig.player.removeLook(0).ok());
    CHECK(rig.player.looks().size() == 1);
    CHECK(rig.player.removeLook(3).error().code == ErrorCode::NotFound);
}

TEST_CASE("Player/Looks: Dateikreislauf und Rückgängig lassen die Liste in Ruhe", "[player][live]") {
    Rig rig;
    REQUIRE(rig.player.saveLook("Extra", 0.0).ok());
    const auto path = (std::filesystem::temp_directory_path() / "wh_looks_roundtrip.whfx").string();
    REQUIRE(rig.player.save(path).ok());
    Rig other;
    REQUIRE(other.player.load(path).ok());
    std::filesystem::remove(path);
    REQUIRE(other.player.looks().size() == 3);
    CHECK(other.player.looks()[2].name == "Extra");
    CHECK(other.player.looks()[2].fadeSeconds == Approx(0.0));

    // Eine Parameteränderung rückgängig machen löscht keinen Look
    const NodeId plasma = rig.player.project()->graph.nodes().front().id;
    REQUIRE(rig.player.setParameter(plasma, "scale", 3.0).ok());
    REQUIRE(rig.player.saveLook("Nachher").ok());
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.looks().size() == 4);
    REQUIRE(rig.player.redo().ok());
    CHECK(rig.player.looks().size() == 4);
}

TEST_CASE("Player/Looks: Look in den Editor laden", "[player][live]") {
    Rig rig;
    REQUIRE(rig.player.loadLookIntoEditor(1).ok());
    CHECK(rig.player.project()->graph.nodes().size() == 1);
    CHECK(rig.player.project()->graph.findNode(rig.player.project()->target->node)->typeId == "wh.generator.noise");
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->graph.nodes().size() == 2);
    CHECK(rig.player.loadLookIntoEditor(9).error().code == ErrorCode::NotFound);
}

TEST_CASE("Player/Live: starten, vorladen, überblenden", "[player][live]") {
    Rig rig;
    CHECK(rig.player.liveGo().error().code == ErrorCode::InvalidArgument);  // nicht live
    REQUIRE(rig.player.start().ok());
    REQUIRE(rig.player.step().ok());

    REQUIRE(rig.player.startLive(0).ok());
    CHECK(rig.player.liveActive());
    REQUIRE(rig.player.step().ok());  // die Session wird mit dem Live-Graphen neu aufgebaut
    CHECK(rig.player.status().running);
    CHECK(rig.player.liveStatus().current == 0);

    CHECK(rig.player.liveGo().error().code == ErrorCode::InvalidArgument);  // nichts vorgeladen
    REQUIRE(rig.player.liveArm(1).ok());
    CHECK(rig.player.liveStatus().armed == 1);
    REQUIRE(rig.player.step().ok());

    REQUIRE(rig.player.liveGo(2.0).ok());
    CHECK(rig.player.liveStatus().fading);
    rig.clock = 10.0;
    REQUIRE(rig.player.step().ok());  // der erste Schritt nach GO zählt als Zeitpunkt 0
    CHECK(rig.player.liveStatus().progress == Approx(0.0));
    rig.clock = 11.0;
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.liveStatus().progress == Approx(0.5));
    rig.clock = 12.5;
    REQUIRE(rig.player.step().ok());
    CHECK_FALSE(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().current == 1);
    CHECK(rig.player.status().running);
    CHECK_FALSE(rig.player.status().lastError.has_value());
}

TEST_CASE("Player/Live: Trigger nimmt die Zeit des Looks, Liste ist gesperrt, Beenden", "[player][live]") {
    Rig rig;
    CHECK(rig.player.startLive(7).error().code == ErrorCode::NotFound);
    REQUIRE(rig.player.startLive(0).ok());
    REQUIRE(rig.player.liveTrigger(1).ok());  // Look "Schnell" hat 0,5 s
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(0.5));
    CHECK(rig.player.removeLook(0).error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.moveLook(0, 1).error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.loadLookIntoEditor(0).error().code == ErrorCode::InvalidArgument);
    CHECK(rig.player.renameLook(1, "Blitz").ok());  // Umbenennen ändert keinen Index

    rig.player.stopLive();
    CHECK_FALSE(rig.player.liveActive());
    CHECK(rig.player.liveStatus().current == -1);

    // Ein neues Projekt beendet den Live-Betrieb von selbst
    REQUIRE(rig.player.startLive(1).ok());
    REQUIRE(rig.player.loadText(kProject).ok());
    CHECK_FALSE(rig.player.liveActive());
}

TEST_CASE("Player/Live: ohne Looks kein Live-Betrieb", "[player][live]") {
    Rig rig;
    REQUIRE(rig.player.removeLook(1).ok());
    REQUIRE(rig.player.removeLook(0).ok());
    CHECK(rig.player.startLive().error().code == ErrorCode::InvalidArgument);
}
