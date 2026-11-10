// Pong: die Spielsimulation (pong_sim) und der Rechen-Node im Evaluator.
//
// Die Simulation rechnet in festen Schritten und kennt keine Uhr; die Tests spielen sie mit 60 Bildern je
// Sekunde durch und prüfen Regeln (Ball bleibt im Feld, Tempo im Taktmodus, Punkte, Spielende), nicht einzelne Pixel.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "../../libs/wh_nodes/src/pong_sim.hpp"
#include "builtin_lab.hpp"

using namespace whitehole;
using whitehole::nodes::detail::PongEvents;
using whitehole::nodes::detail::PongSettings;
using whitehole::nodes::detail::PongSim;
using whitehole::nodes::detail::PongView;
using whitehole::test::Lab;
using Catch::Approx;

namespace {

constexpr double kFrame = 1.0 / 60.0;

struct Played {
    PongView view;
    int paddleHits{0};
    int wallHits{0};
    int points{0};
    double minY{1.0};
    double maxY{0.0};
};

/// Spielt `seconds` Sekunden; `serve` gilt nur für das erste Bild.
Played play(PongSim& sim, const PongSettings& s, double seconds, bool serve = false) {
    Played out;
    const int frames = static_cast<int>(std::lround(seconds / kFrame));
    for (int i = 0; i < frames; ++i) {
        PongEvents events;
        out.view = sim.step(kFrame, s, serve && i == 0, false, events);
        out.paddleHits += events.paddleHit ? 1 : 0;
        out.wallHits += events.wallHit ? 1 : 0;
        out.points += events.point ? 1 : 0;
        out.minY = std::min(out.minY, out.view.ballY);
        out.maxY = std::max(out.maxY, out.view.ballY);
    }
    return out;
}

PongSettings steady() {
    PongSettings s;
    s.skill = 1.0;
    s.speedUp = 0.0;
    s.paddleSpeed = 6.0;
    s.maxScore = 15;
    return s;
}

}  // namespace

TEST_CASE("Pong: gleiche Einstellungen und Seed ergeben dasselbe Spiel", "[nodes][pong]") {
    PongSettings s;
    s.seed = 7;
    PongSim a;
    PongSim b;
    const Played x = play(a, s, 20.0);
    const Played y = play(b, s, 20.0);
    CHECK(x.view.ballX == y.view.ballX);
    CHECK(x.view.ballY == y.view.ballY);
    CHECK(x.view.scoreLeft == y.view.scoreLeft);
    CHECK(x.paddleHits == y.paddleHits);
}

TEST_CASE("Pong: wartet vor dem Aufschlag in der Mitte und bleibt im Feld", "[nodes][pong]") {
    PongSim sim;
    PongSettings s = steady();
    const Played start = play(sim, s, 0.3);
    CHECK(start.view.flying == false);
    CHECK(start.view.ballX == Approx(0.5));
    CHECK(start.view.ballY == Approx(0.5));

    const Played run = play(sim, s, 20.0);
    CHECK(run.minY >= s.ballRadius - 1e-9);
    CHECK(run.maxY <= 1.0 - s.ballRadius + 1e-9);
    CHECK(run.wallHits > 0);
}

TEST_CASE("Pong: ein Rechner mit Geschick 1 hält den Ballwechsel am Laufen", "[nodes][pong]") {
    PongSim sim;
    const Played run = play(sim, steady(), 40.0);
    CHECK(run.points == 0);
    CHECK(run.paddleHits >= 20);
    CHECK(run.view.scoreLeft == 0);
    CHECK(run.view.scoreRight == 0);
}

TEST_CASE("Pong: ohne Geschick fällt jeder Ballwechsel, am Ende gewinnt einer und das Spiel beginnt neu", "[nodes][pong]") {
    PongSim sim;
    PongSettings s = steady();
    s.skill = 0.0;
    s.maxScore = 2;
    s.winHold = 1.0;
    s.serveDelay = 0.2;

    bool sawWinner = false;
    int winner = 0;
    double flashLow = 1.0;
    int points = 0;
    for (int i = 0; i < 60 * 30 && !sawWinner; ++i) {
        PongEvents events;
        const PongView v = sim.step(kFrame, s, false, false, events);
        points += events.point ? 1 : 0;
        if (v.winner != 0) {
            sawWinner = true;
            winner = v.winner;
            CHECK(std::max(v.scoreLeft, v.scoreRight) == 2);
            for (int k = 0; k < 20; ++k) {
                flashLow = std::min(flashLow, sim.step(kFrame, s, false, false, events).flash);
            }
        }
    }
    CHECK(sawWinner);
    CHECK((winner == 1 || winner == 2));
    CHECK(points >= 2);
    CHECK(points <= 3);  // bei 2 Punkten zum Sieg höchstens 3 Ballwechsel
    CHECK(flashLow == Approx(0.25));

    PongEvents events;
    PongView after;
    for (int i = 0; i < 60 * 2; ++i) {
        after = sim.step(kFrame, s, false, false, events);
    }
    CHECK(after.winner == 0);  // nach der Siegerpause neues Spiel
    CHECK(after.scoreLeft + after.scoreRight <= 1);
}

TEST_CASE("Pong: ohne automatischen Aufschlag wartet der Ball auf das Signal", "[nodes][pong]") {
    PongSim sim;
    PongSettings s = steady();
    s.autoServe = false;
    CHECK(play(sim, s, 3.0).view.flying == false);
    CHECK(play(sim, s, 0.1, true).view.flying == true);
}

TEST_CASE("Pong: ein Schläger mit Eingang folgt dem Wert", "[nodes][pong]") {
    PongSim sim;
    PongSettings s = steady();
    s.leftControl = 1;
    s.leftInput = 0.9;
    s.autoServe = false;
    const Played run = play(sim, s, 0.2);
    CHECK(run.view.left == Approx(0.875));  // 0,9 wird auf das Feld begrenzt: 1 - Größe / 2
    s.leftInput = 0.0;
    CHECK(play(sim, s, 0.1).view.left == Approx(0.125));
}

TEST_CASE("Pong: im Taktmodus dauert eine Überquerung genau die eingestellten Schläge", "[nodes][pong]") {
    PongSim sim;
    PongSettings s = steady();
    s.beatMode = true;
    s.bpm = 120.0;
    s.beatsPerCrossing = 2.0;  // 1 s von Schläger zu Schläger

    // Ohne Schlag bleibt der Ball in der Mitte.
    CHECK(play(sim, s, 2.0).view.flying == false);

    // Auf den Schlag geht es los.
    PongEvents events;
    PongView v = sim.step(kFrame, s, false, true, events);
    CHECK(v.flying);

    std::vector<double> hitTimes;
    double t = kFrame;
    for (int i = 0; i < 60 * 8; ++i, t += kFrame) {
        PongEvents e;
        sim.step(kFrame, s, false, false, e);
        if (e.paddleHit) {
            hitTimes.push_back(t);
        }
    }
    REQUIRE(hitTimes.size() >= 4);
    for (std::size_t i = 1; i < hitTimes.size(); ++i) {
        CHECK(hitTimes[i] - hitTimes[i - 1] == Approx(1.0).margin(0.03));
    }
}

TEST_CASE("Pong: der Rechen-Node liefert Stand und Ereignisse", "[nodes][pong]") {
    Lab lab;
    const NodeId pong = lab.add("wh.value.pong");
    lab.feed(pong, "ballX");
    lab.set(pong, "skill", 1.0);
    lab.set(pong, "speedUp", 0.0);
    lab.set(pong, "paddleSpeed", 6.0);
    lab.set(pong, "maxScore", std::int64_t{15});

    lab.run(0.0, 0.0);
    CHECK(lab.number(pong, "ballX") == Approx(0.5));
    CHECK(lab.number(pong, "paddleLength") == Approx(0.25));
    CHECK(std::get<std::int64_t>(*lab.store.value(pong, "target")) == 15);

    bool paddleHit = false;
    bool wallHit = false;
    double t = 0.0;
    for (int i = 0; i < 60 * 20; ++i) {
        t += kFrame;
        lab.run(t, kFrame);
        paddleHit = paddleHit || std::get<bool>(*lab.store.value(pong, "hitPaddle"));
        wallHit = wallHit || std::get<bool>(*lab.store.value(pong, "hitWall"));
        const double y = lab.number(pong, "ballY");
        REQUIRE(y >= 0.0);
        REQUIRE(y <= 1.0);
    }
    CHECK(paddleHit);
    CHECK(wallHit);
    CHECK(std::get<std::int64_t>(*lab.store.value(pong, "scoreLeft")) == 0);
}
