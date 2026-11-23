// Manuelle Überblendung der Live-Decks (Fader am MIDI-Controller): LiveDecks::setManualProgress.
//
// Warum eigene Datei: live_decks_test.cpp ist schon lang; hier stehen die Randfälle der Fader-Bedienung
// (nichts vorgeladen, zurückfahren, go() mittendrin, arm() mittendrin, Uhr läuft nicht mit).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "builtin_lab.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/live_decks.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

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

look Eins fade 3
node a wh.generator.plasma
end
output a.out
endlook

look Zwei fade 1
node b wh.generator.plasma
end
output b.out
endlook
)";

double amountOf(const runtime::LiveDecks& decks) {
    const graph::NodeInstance* mixer = decks.graph().findNode(decks.target().node);
    REQUIRE(mixer != nullptr);
    return std::get<double>(mixer->parameters.at("amount"));
}

struct Rig {
    Lab lab;
    project::Project project = project::parseProject(kProject, lab.registry).value();
    runtime::LiveDecks decks{lab.registry};
    Rig() { REQUIRE(decks.start(project.looks[0], 0).ok()); }
};

}  // namespace

TEST_CASE("Manueller Fade: ohne vorgeladenen Look wirkungslos", "[live][manual]") {
    Rig rig;
    CHECK_FALSE(rig.decks.setManualProgress(0.5));
    CHECK_FALSE(rig.decks.status().fading);
    CHECK(amountOf(rig.decks) == Approx(0.0));
}

TEST_CASE("Manueller Fade: Fader hoch, die Uhr läuft nicht mit, bei 1 fertig", "[live][manual]") {
    Rig rig;
    REQUIRE(rig.decks.arm(rig.project.looks[1], 1).ok());
    CHECK(rig.decks.setManualProgress(0.3));
    CHECK(rig.decks.status().fading);
    CHECK(rig.decks.status().manual);
    CHECK(rig.decks.status().incoming == 1);
    CHECK(amountOf(rig.decks) == Approx(0.3));
    CHECK_FALSE(rig.decks.advance(10.0));  // die Zeit schiebt nicht weiter
    CHECK(amountOf(rig.decks) == Approx(0.3));
    CHECK(rig.decks.setManualProgress(0.8));
    CHECK(amountOf(rig.decks) == Approx(0.8));
    CHECK(rig.decks.setManualProgress(1.0));
    CHECK_FALSE(rig.decks.status().fading);
    CHECK_FALSE(rig.decks.status().manual);
    CHECK(rig.decks.status().current == 1);
    CHECK(rig.decks.status().armed == -1);
    CHECK(amountOf(rig.decks) == Approx(1.0));
    CHECK_FALSE(rig.decks.setManualProgress(0.5));  // nichts mehr vorgeladen
}

TEST_CASE("Manueller Fade: zurück auf 0 lässt den Look vorgeladen", "[live][manual]") {
    Rig rig;
    REQUIRE(rig.decks.arm(rig.project.looks[1], 1).ok());
    REQUIRE(rig.decks.setManualProgress(0.6));
    CHECK(rig.decks.setManualProgress(0.2));  // rückwärts
    CHECK(amountOf(rig.decks) == Approx(0.2));
    CHECK(rig.decks.setManualProgress(0.0));
    CHECK_FALSE(rig.decks.status().fading);
    CHECK(rig.decks.status().armed == 1);
    CHECK(rig.decks.status().current == 0);
    CHECK(amountOf(rig.decks) == Approx(0.0));
    CHECK_FALSE(rig.decks.setManualProgress(0.0));  // schon dort
}

TEST_CASE("Manueller Fade: in Richtung B -> A (zweiter Wechsel) fällt der Mischwert", "[live][manual]") {
    Rig rig;
    REQUIRE(rig.decks.arm(rig.project.looks[1], 1).ok());
    REQUIRE(rig.decks.setManualProgress(1.0));
    REQUIRE(rig.decks.arm(rig.project.looks[0], 0).ok());
    REQUIRE(rig.decks.setManualProgress(0.25));
    CHECK(amountOf(rig.decks) == Approx(0.75));
    REQUIRE(rig.decks.setManualProgress(1.0));
    CHECK(rig.decks.status().current == 0);
    CHECK(amountOf(rig.decks) == Approx(0.0));
}

TEST_CASE("Manueller Fade: go() mittendrin blendet mit der Uhr zu Ende", "[live][manual]") {
    Rig rig;
    REQUIRE(rig.decks.arm(rig.project.looks[1], 1).ok());
    REQUIRE(rig.decks.setManualProgress(0.5));
    REQUIRE(rig.decks.go(2.0).ok());
    CHECK(rig.decks.status().fading);
    CHECK_FALSE(rig.decks.status().manual);
    CHECK(rig.decks.advance(0.5));  // 0.5 + 0.5/2
    CHECK(amountOf(rig.decks) == Approx(0.75));
    CHECK_FALSE(rig.decks.setManualProgress(0.1));  // ein Zeit-Fade gehört nicht dem Fader
    CHECK(rig.decks.advance(1.0));
    CHECK(rig.decks.status().current == 1);
}

TEST_CASE("Manueller Fade: arm() mittendrin schließt den Fade ab", "[live][manual]") {
    Rig rig;
    REQUIRE(rig.decks.arm(rig.project.looks[1], 1).ok());
    REQUIRE(rig.decks.setManualProgress(0.4));
    REQUIRE(rig.decks.arm(rig.project.looks[0], 0).ok());
    CHECK(rig.decks.status().current == 1);
    CHECK(rig.decks.status().armed == 0);
    CHECK_FALSE(rig.decks.status().manual);
}

TEST_CASE("Manueller Fade: ungültige Werte", "[live][manual]") {
    Rig rig;
    REQUIRE(rig.decks.arm(rig.project.looks[1], 1).ok());
    CHECK_FALSE(rig.decks.setManualProgress(std::nan("")));
    CHECK(rig.decks.setManualProgress(-3.0) == false);  // wie 0, war nicht im Fade
    CHECK(rig.decks.setManualProgress(7.0));  // wie 1
    CHECK(rig.decks.status().current == 1);
}
