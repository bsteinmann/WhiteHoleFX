// Math, Compare, Clock, Envelope und Sequencer einzeln und im Zusammenspiel.
//
// Die zeitabhängigen Nodes rechnen nur mit dem Zeitschritt des Frames; die Tests geben ihn von Hand
// vor und wählen Schritte, die sich in Binärbrüchen exakt darstellen lassen (0.125, 0.25), damit
// Schwellen wie "die Phase erreicht 1" nicht von Rundungsfehlern abhängen.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <variant>
#include <vector>

#include "builtin_lab.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

namespace {

bool flag(const Lab& lab, NodeId node, const char* port) {
    const auto value = lab.store.value(node, port);
    REQUIRE(value.has_value());
    return std::get<bool>(*value);
}

std::int64_t whole(const Lab& lab, NodeId node, const char* port) {
    const auto value = lab.store.value(node, port);
    REQUIRE(value.has_value());
    return std::get<std::int64_t>(*value);
}

double math(Lab& lab, NodeId node, std::int64_t op, double a, double b) {
    lab.set(node, "op", op);
    lab.set(node, "a", a);
    lab.set(node, "b", b);
    return lab.sample(node, 0.0);
}

/// Schaltet `advance` an und wieder aus, ein Frame je Zustand; danach zeigt der Store den Frame "an".
void pulse(Lab& lab, NodeId node, const char* key) {
    lab.set(node, key, false);
    lab.run(0.0, 0.125);
    lab.set(node, key, true);
    lab.run(0.0, 0.125);
}

}  // namespace

TEST_CASE("Math: alle Rechenarten") {
    Lab lab;
    const NodeId node = lab.add("wh.value.math");
    lab.feed(node, "out");

    CHECK(math(lab, node, 0, 2.0, 3.0) == Approx(5.0));    // Add
    CHECK(math(lab, node, 1, 2.0, 3.0) == Approx(-1.0));   // Subtract
    CHECK(math(lab, node, 2, 2.0, 3.0) == Approx(6.0));    // Multiply
    CHECK(math(lab, node, 3, 3.0, 2.0) == Approx(1.5));    // Divide
    CHECK(math(lab, node, 4, 2.0, 3.0) == Approx(2.0));    // Min
    CHECK(math(lab, node, 5, 2.0, 3.0) == Approx(3.0));    // Max
    CHECK(math(lab, node, 6, 2.0, 3.0) == Approx(8.0));    // Power
    CHECK(math(lab, node, 7, 7.0, 3.0) == Approx(1.0));    // Modulo
    CHECK(math(lab, node, 7, -1.0, 3.0) == Approx(2.0));   // Modulo mit negativem A bleibt im Bereich 0..B
    CHECK(math(lab, node, 8, -4.5, 0.0) == Approx(4.5));   // Absolute
    CHECK(math(lab, node, 9, 0.37, 0.25) == Approx(0.25)); // Quantize
    CHECK(math(lab, node, 9, 0.38, 0.25) == Approx(0.5));
    CHECK(math(lab, node, 10, 0.25, 2.0) == Approx(2.0));  // Sine: eine Viertelumdrehung, Höhe 2
}

TEST_CASE("Math: Fehlerfälle liefern 0 statt NaN") {
    Lab lab;
    const NodeId node = lab.add("wh.value.math");
    lab.feed(node, "out");
    CHECK(math(lab, node, 3, 5.0, 0.0) == 0.0);     // Teilen durch 0
    CHECK(math(lab, node, 7, 5.0, 0.0) == 0.0);     // Modulo 0
    CHECK(math(lab, node, 6, -8.0, 0.5) == 0.0);    // Wurzel aus Negativem
    CHECK(math(lab, node, 6, 10.0, 1000.0) == 0.0); // Überlauf
    CHECK(math(lab, node, 9, 0.7, 0.0) == Approx(0.7));  // Quantize ohne Schrittweite lässt den Wert
}

TEST_CASE("Compare: Schwelle, Hysterese und Flanken") {
    Lab lab;
    const NodeId node = lab.add("wh.value.compare");
    lab.feed(node, "out");
    lab.set(node, "threshold", 0.5);
    lab.set(node, "hysteresis", 0.2);  // an über 0.6, aus unter 0.4

    const auto step = [&](double value) {
        lab.set(node, "value", value);
        lab.run(0.0, 0.1);
    };
    step(0.0);
    CHECK_FALSE(flag(lab, node, "out"));
    step(0.55);  // in der Hysterese: bleibt aus
    CHECK_FALSE(flag(lab, node, "out"));
    CHECK_FALSE(flag(lab, node, "rise"));
    step(0.65);
    CHECK(flag(lab, node, "out"));
    CHECK(flag(lab, node, "rise"));
    CHECK_FALSE(flag(lab, node, "fall"));
    step(0.7);  // bleibt an: die Flanke dauert nur einen Frame
    CHECK(flag(lab, node, "out"));
    CHECK_FALSE(flag(lab, node, "rise"));
    step(0.45);  // in der Hysterese: bleibt an
    CHECK(flag(lab, node, "out"));
    step(0.35);
    CHECK_FALSE(flag(lab, node, "out"));
    CHECK(flag(lab, node, "fall"));
}

TEST_CASE("Compare: Modus Below kehrt um, der erste Frame hat keine Flanke") {
    Lab lab;
    const NodeId node = lab.add("wh.value.compare");
    lab.feed(node, "out");
    lab.set(node, "mode", std::int64_t{1});
    lab.set(node, "value", 0.2);
    lab.run(0.0, 0.1);
    CHECK(flag(lab, node, "out"));  // 0.2 liegt unter 0.5
    CHECK_FALSE(flag(lab, node, "rise"));
    lab.set(node, "value", 0.9);
    lab.run(0.0, 0.1);
    CHECK_FALSE(flag(lab, node, "out"));
    CHECK(flag(lab, node, "fall"));
}

TEST_CASE("Clock: Ticks im Takt, Phase und Zähler") {
    Lab lab;
    const NodeId clock = lab.add("wh.value.clock");
    lab.feed(clock, "phase");
    lab.set(clock, "bpm", 120.0);  // 2 Schläge je Sekunde: 0.25 Phase je Frame zu 0.125 s

    std::vector<int> tickFrames;
    for (int frame = 1; frame <= 9; ++frame) {
        lab.run(0.125 * frame, 0.125);
        if (flag(lab, clock, "tick")) {
            tickFrames.push_back(frame);
        }
        if (frame == 2) {
            CHECK(lab.number(clock, "phase") == Approx(0.5));
            CHECK(whole(lab, clock, "count") == 0);
        }
    }
    CHECK(tickFrames == std::vector<int>{4, 8});
    CHECK(whole(lab, clock, "count") == 2);
    CHECK(lab.number(clock, "phase") == Approx(0.25));
}

TEST_CASE("Clock: Verhältnis, Tempowechsel, Stopp und Reset") {
    Lab lab;
    const NodeId clock = lab.add("wh.value.clock");
    lab.feed(clock, "phase");
    lab.set(clock, "bpm", 120.0);
    lab.set(clock, "ratio", std::int64_t{4});  // 2x: Tick alle 2 Frames
    int ticks = 0;
    for (int frame = 1; frame <= 8; ++frame) {
        lab.run(0.0, 0.125);
        ticks += flag(lab, clock, "tick") ? 1 : 0;
    }
    CHECK(ticks == 4);

    // Tempowechsel: die Phase läuft von ihrem Stand weiter, sie springt nicht
    lab.set(clock, "ratio", std::int64_t{3});
    lab.run(0.0, 0.125);
    const double before = lab.number(clock, "phase");
    lab.set(clock, "bpm", 60.0);
    lab.run(0.0, 0.125);
    CHECK(lab.number(clock, "phase") == Approx(std::fmod(before + 0.125, 1.0)));

    // Stopp: nichts bewegt sich mehr
    lab.set(clock, "running", false);
    const double held = lab.number(clock, "phase");
    lab.run(0.0, 0.125);
    CHECK(lab.number(clock, "phase") == Approx(held));
    CHECK_FALSE(flag(lab, clock, "tick"));

    // Reset: steigende Flanke setzt zurück, ohne Tick
    lab.set(clock, "running", true);
    lab.set(clock, "reset", true);
    lab.run(0.0, 0.125);
    CHECK(lab.number(clock, "phase") == Approx(0.0));
    CHECK(whole(lab, clock, "count") == 0);
    CHECK_FALSE(flag(lab, clock, "tick"));
}

TEST_CASE("Envelope: ADSR folgt dem Gate") {
    Lab lab;
    const NodeId env = lab.add("wh.value.envelope");
    lab.feed(env, "out");
    lab.set(env, "attack", 1.0);
    lab.set(env, "decay", 1.0);
    lab.set(env, "sustain", 0.5);
    lab.set(env, "release", 1.0);

    lab.run(0.0, 0.25);
    CHECK(lab.number(env, "out") == 0.0);
    CHECK_FALSE(flag(lab, env, "active"));

    lab.set(env, "gate", true);
    const std::vector<double> expected{0.25, 0.5, 0.75, 1.0, 0.75, 0.5, 0.5, 0.5};
    for (const double level : expected) {
        lab.run(0.0, 0.25);
        CHECK(lab.number(env, "out") == Approx(level));
        CHECK(flag(lab, env, "active"));
    }

    lab.set(env, "gate", false);
    for (const double level : {0.25, 0.0, 0.0}) {
        lab.run(0.0, 0.25);
        CHECK(lab.number(env, "out") == Approx(level));
    }
    CHECK_FALSE(flag(lab, env, "active"));
}

TEST_CASE("Envelope: neuer Anschlag setzt vom aktuellen Pegel an, Zeit 0 ist sofort") {
    Lab lab;
    const NodeId env = lab.add("wh.value.envelope");
    lab.feed(env, "out");
    lab.set(env, "attack", 1.0);
    lab.set(env, "release", 1.0);
    lab.set(env, "sustain", 1.0);

    lab.set(env, "gate", true);
    for (int i = 0; i < 2; ++i) {
        lab.run(0.0, 0.25);
    }
    CHECK(lab.number(env, "out") == Approx(0.5));
    lab.set(env, "gate", false);
    lab.run(0.0, 0.25);
    CHECK(lab.number(env, "out") == Approx(0.25));
    lab.set(env, "gate", true);  // Retrigger mitten im Ausklingen: kein Sprung nach 0
    lab.run(0.0, 0.25);
    CHECK(lab.number(env, "out") == Approx(0.5));

    lab.set(env, "attack", 0.0);
    lab.set(env, "gate", false);
    lab.run(0.0, 0.25);
    lab.set(env, "gate", true);
    lab.run(0.0, 0.25);
    CHECK(lab.number(env, "out") == Approx(1.0));
}

TEST_CASE("Envelope: One-shot macht aus einem Impuls Anschlag und Ausklingen") {
    Lab lab;
    const NodeId env = lab.add("wh.value.envelope");
    lab.feed(env, "out");
    lab.set(env, "mode", std::int64_t{1});
    lab.set(env, "attack", 0.5);
    lab.set(env, "release", 0.5);

    lab.set(env, "gate", true);
    lab.run(0.0, 0.25);
    lab.set(env, "gate", false);  // der Impuls ist nur einen Frame lang
    const std::vector<double> expected{1.0, 0.5, 0.0};
    // Frame 1 (Gate an) stieg auf 0.5; dann Spitze 1.0, ab dem Folgeframe Ausklingen
    CHECK(lab.number(env, "out") == Approx(0.5));
    for (const double level : expected) {
        lab.run(0.0, 0.25);
        CHECK(lab.number(env, "out") == Approx(level));
    }
    CHECK_FALSE(flag(lab, env, "active"));
}

TEST_CASE("Sequencer: Schritte, Wert, Trigger und Hit") {
    Lab lab;
    const NodeId seq = lab.add("wh.value.sequencer");
    lab.feed(seq, "value");
    lab.set(seq, "steps", std::int64_t{4});  // Werte 1, 0, 0.5, 0

    lab.run(0.0, 0.125);
    CHECK(lab.number(seq, "value") == Approx(1.0));
    CHECK(whole(lab, seq, "step") == 0);
    CHECK_FALSE(flag(lab, seq, "trigger"));  // der Start ist kein Weiterschalten

    const std::vector<double> values{0.0, 0.5, 0.0, 1.0};
    const std::vector<bool> hits{false, true, false, true};
    for (std::size_t i = 0; i < values.size(); ++i) {
        pulse(lab, seq, "advance");
        CHECK(lab.number(seq, "value") == Approx(values[i]));
        CHECK(whole(lab, seq, "step") == static_cast<std::int64_t>((i + 1) % 4));
        CHECK(flag(lab, seq, "trigger"));
        CHECK(flag(lab, seq, "hit") == hits[i]);
    }
    lab.run(0.0, 0.125);  // Gate bleibt an: kein weiterer Schritt
    CHECK(whole(lab, seq, "step") == 0);
    CHECK_FALSE(flag(lab, seq, "trigger"));
}

TEST_CASE("Sequencer: Richtungen und Reset") {
    Lab lab;
    const NodeId seq = lab.add("wh.value.sequencer");
    lab.feed(seq, "value");
    lab.set(seq, "steps", std::int64_t{4});
    lab.run(0.0, 0.125);

    const auto walk = [&](std::int64_t direction, int count) {
        lab.set(seq, "reset", false);
        lab.set(seq, "direction", direction);
        lab.run(0.0, 0.125);
        lab.set(seq, "reset", true);
        lab.run(0.0, 0.125);
        std::vector<std::int64_t> visited{whole(lab, seq, "step")};
        for (int i = 0; i < count; ++i) {
            pulse(lab, seq, "advance");
            visited.push_back(whole(lab, seq, "step"));
        }
        lab.set(seq, "advance", false);
        return visited;
    };
    CHECK(walk(0, 5) == std::vector<std::int64_t>{0, 1, 2, 3, 0, 1});
    CHECK(walk(1, 5) == std::vector<std::int64_t>{3, 2, 1, 0, 3, 2});  // rückwärts beginnt am letzten Schritt
    CHECK(walk(2, 7) == std::vector<std::int64_t>{0, 1, 2, 3, 2, 1, 0, 1});
    const std::vector<std::int64_t> random = walk(3, 12);
    CHECK(random.front() == 0);
    for (const std::int64_t step : random) {
        CHECK(step >= 0);
        CHECK(step < 4);
    }
    CHECK(walk(3, 12) == random);  // gleicher Seed, gleiche Folge
    lab.set(seq, "seed", std::int64_t{5});
    CHECK(walk(3, 12) != random);
}

TEST_CASE("Sequencer: eine Steuerung durch Clock, Tick schaltet weiter") {
    Lab lab;
    const NodeId clock = lab.add("wh.value.clock");
    const NodeId seq = lab.add("wh.value.sequencer");
    lab.set(clock, "bpm", 120.0);
    lab.set(seq, "steps", std::int64_t{4});
    lab.connect(clock, "tick", seq, "advance");
    lab.feed(seq, "value");

    // Der Tick liegt einen Frame lang an; die steigende Flanke schaltet genau einmal weiter.
    std::vector<std::int64_t> steps;
    for (int frame = 1; frame <= 12; ++frame) {
        lab.run(0.0, 0.125);
        steps.push_back(whole(lab, seq, "step"));
    }
    CHECK(steps == std::vector<std::int64_t>{0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3});
}
