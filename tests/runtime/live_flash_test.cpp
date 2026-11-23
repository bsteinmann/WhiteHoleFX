// Looks drücken und loslassen: umschalten und flashen (Player::liveDown/liveUp), dazu `trigger`/`return` in der
// Projektdatei. Entwurf: docs/entwurf-midi.md, Abschnitt 5.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
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
output plasma.out

look Basis fade 3
node a wh.generator.plasma
end
output a.out
endlook

look Blitz fade 0 trigger flash
node b wh.generator.noise
end
output b.out
endlook

look Strobe fade 0.5 trigger flash return 0
node c wh.generator.plasma
end
output c.out
endlook

look Anders fade 1
node d wh.generator.noise
end
output d.out
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
        REQUIRE(player.start().ok());
        REQUIRE(player.startLive(0).ok());
        REQUIRE(player.step().ok());
    }
    void run(double seconds) {
        clock += seconds;
        REQUIRE(player.step().ok());
    }
    [[nodiscard]] int shown() const { return player.liveStatus().fading ? player.liveStatus().incoming : player.liveStatus().current; }
};

}  // namespace

TEST_CASE("Projektdatei: trigger und return am Look", "[live][flash]") {
    Rig rig;
    const auto& looks = rig.player.looks();
    CHECK(looks[0].trigger == project::LookTrigger::Toggle);
    CHECK_FALSE(looks[0].returnSeconds.has_value());
    CHECK(looks[1].trigger == project::LookTrigger::Flash);
    CHECK_FALSE(looks[1].returnSeconds.has_value());
    CHECK(looks[2].returnSeconds == Approx(0.0));
    const auto text = project::writeProject(*rig.player.project());
    REQUIRE(text.ok());
    CHECK(text.value().find("look Basis fade 3\n") != std::string::npos);  // Standard wird nicht geschrieben
    CHECK(text.value().find("look Blitz fade 0 trigger flash\n") != std::string::npos);
    CHECK(text.value().find("look Strobe fade 0.5 trigger flash return 0\n") != std::string::npos);

    const auto bad = [&](const std::string& head) {
        const std::string project = std::string{"whitehole-project 1\n"} + head + "\nnode n wh.generator.plasma\nend\noutput n.out\nendlook\n";
        CHECK_FALSE(project::parseProject(project, rig.library->registry).ok());
    };
    bad("look X trigger hold");
    bad("look X return -1");
    bad("look X return 601");
}

TEST_CASE("Flash: Drücken blendet ein, Loslassen mit der Zeit des Looks zurück", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.liveDown(1).ok());
    rig.run(0.1);
    CHECK(rig.shown() == 1);
    CHECK(rig.player.liveFlashHeld() == std::vector<int>{1});
    REQUIRE(rig.player.liveUp(1).ok());
    CHECK(rig.player.liveStatus().current == 0);  // "Blitz" hat fade 0 und kein return: sofort zurück
    CHECK_FALSE(rig.player.liveStatus().fading);
    rig.run(0.1);
    CHECK(rig.shown() == 0);
    CHECK(rig.player.liveFlashHeld().empty());
}

TEST_CASE("Flash: eigene Zeit fürs Zurückblenden", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.liveDown(2).ok());
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(0.5));  // hin mit fade
    rig.run(1.0);
    REQUIRE(rig.player.liveUp(2).ok());
    rig.run(0.0);
    CHECK(rig.player.liveStatus().current == 0);  // return 0: sofort zurück
}

TEST_CASE("Flash hat Vorrang vor Mit GO, umschalten folgt GO", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.liveDown(3, std::nullopt, true).ok());  // umschaltend, Mit GO: nur vorladen
    CHECK(rig.player.liveStatus().armed == 3);
    CHECK_FALSE(rig.player.liveStatus().fading);
    REQUIRE(rig.player.liveDown(1, std::nullopt, true).ok());  // Flash trotz GO sofort
    rig.run(0.1);
    CHECK(rig.shown() == 1);
}

TEST_CASE("Flash: Loslassen eines umschaltenden Looks tut nichts", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.liveDown(3).ok());
    rig.run(2.0);
    REQUIRE(rig.player.liveUp(3).ok());
    rig.run(0.1);
    CHECK(rig.shown() == 3);
}

TEST_CASE("Flash: zwei gehalten - erst das letzte Loslassen kehrt zurück", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.liveDown(1).ok());
    rig.run(0.1);
    REQUIRE(rig.player.liveDown(2).ok());
    rig.run(1.0);
    CHECK(rig.shown() == 2);
    REQUIRE(rig.player.liveUp(1).ok());  // der untere: Bild bleibt
    rig.run(0.1);
    CHECK(rig.shown() == 2);
    REQUIRE(rig.player.liveUp(2).ok());
    rig.run(0.1);
    CHECK(rig.shown() == 0);  // zum Look vor dem ersten Flash

    // Oberen zuerst loslassen: zurück zum darunter gehaltenen
    REQUIRE(rig.player.liveDown(1).ok());
    REQUIRE(rig.player.liveDown(2).ok());
    rig.run(1.0);
    REQUIRE(rig.player.liveUp(2).ok());
    rig.run(0.1);
    CHECK(rig.shown() == 1);
    REQUIRE(rig.player.liveUp(1).ok());
    rig.run(0.1);
    CHECK(rig.shown() == 0);
}

TEST_CASE("Flash: Umschalten während des Haltens beendet das Flash", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.liveDown(1).ok());
    rig.run(0.1);
    REQUIRE(rig.player.liveTrigger(3).ok());
    rig.run(2.0);
    CHECK(rig.player.liveFlashHeld().empty());
    REQUIRE(rig.player.liveUp(1).ok());
    rig.run(0.1);
    CHECK(rig.shown() == 3);  // bleibt stehen
}

TEST_CASE("Flash: Überschreiben eines Looks behält trigger und return", "[live][flash]") {
    Rig rig;
    rig.player.stopLive();
    REQUIRE(rig.player.saveLook("Strobe").ok());
    CHECK(rig.player.looks()[2].trigger == project::LookTrigger::Flash);
    CHECK(rig.player.looks()[2].returnSeconds == Approx(0.0));
}

TEST_CASE("Flash: ohne return gilt die Überblendzeit des geflashten Looks", "[live][flash]") {
    Rig rig;
    rig.player.stopLive();
    REQUIRE(rig.player.setLookFade(1, 2.0).ok());
    REQUIRE(rig.player.startLive(0).ok());
    REQUIRE(rig.player.step().ok());
    REQUIRE(rig.player.liveDown(1).ok());
    rig.run(3.0);
    REQUIRE(rig.player.liveUp(1).ok());
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.liveStatus().incoming == 0);
    CHECK(rig.player.liveStatus().fadeSeconds == Approx(2.0));
}

TEST_CASE("setLookTrigger: umstellen, prüfen, im Live-Betrieb erlaubt", "[live][flash]") {
    Rig rig;
    REQUIRE(rig.player.setLookTrigger(3, project::LookTrigger::Flash, 1.5).ok());
    CHECK(rig.player.looks()[3].trigger == project::LookTrigger::Flash);
    CHECK(rig.player.looks()[3].returnSeconds == Approx(1.5));
    CHECK(rig.player.modified());
    CHECK_FALSE(rig.player.setLookTrigger(3, project::LookTrigger::Flash, -1.0).ok());
    CHECK_FALSE(rig.player.setLookTrigger(9, project::LookTrigger::Toggle, std::nullopt).ok());
    REQUIRE(rig.player.setLookTrigger(3, project::LookTrigger::Toggle, std::nullopt).ok());
    CHECK_FALSE(rig.player.looks()[3].returnSeconds.has_value());
}
