// Node "Tempo" (wh.value.tempo): liest die gemeinsame Tempo-Uhr; zwei Nodes (z. B. in zwei Looks) sehen denselben Stand,
// und ein neu angelegter Node kennt das Tempo sofort, statt von vorn zu zählen.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "builtin_lab.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

TEST_CASE("Tempo-Node: BPM, Beat, Phase und Zähler aus der gemeinsamen Uhr", "[nodes][tempo]") {
    Lab lab;
    lab.tempo->setFixedBpm(60.0);
    lab.tempo->setSource(values::TempoClock::Source::Fixed);
    const NodeId first = lab.add("wh.value.tempo");
    lab.feed(first, "phase");
    lab.tempo->update(5.0);
    lab.run(0.0);
    CHECK(lab.number(first, "bpm") == Approx(60.0));
    CHECK(std::get<bool>(*lab.store.value(first, "beat")));

    lab.tempo->update(5.5);
    lab.run(0.5);
    CHECK(lab.number(first, "phase") == Approx(0.5));
    CHECK_FALSE(std::get<bool>(*lab.store.value(first, "beat")));
    CHECK(lab.number(first, "pulse") == Approx(std::exp(-0.5 / 0.25)));

    // Ein später angelegter Node (ein neuer Look) zählt nicht von vorn: derselbe Stand wie der erste
    const NodeId second = lab.add("wh.value.tempo");
    lab.feed(second, "count");
    lab.tempo->update(7.25);
    lab.run(2.25);
    CHECK(std::get<std::int64_t>(*lab.store.value(second, "count")) == 2);
    CHECK(lab.number(second, "phase") == Approx(lab.number(first, "phase")));
    CHECK(lab.number(second, "phase") == Approx(0.25));
}

TEST_CASE("Globales Tempo: Clock folgt Phase und Zähler der gemeinsamen Uhr", "[nodes][tempo]") {
    Lab lab;
    lab.tempo->setFixedBpm(60.0);
    lab.tempo->setSource(values::TempoClock::Source::Fixed);
    const NodeId clock = lab.add("wh.value.clock");
    lab.set(clock, "globalTempo", true);
    lab.set(clock, "ratio", std::int64_t{4});  // 2x: zwei Ticks je Beat
    lab.set(clock, "bpm", 300.0);              // zählt mit globalem Tempo nicht
    lab.feed(clock, "phase");
    lab.tempo->update(0.0);
    lab.run(0.0);
    CHECK_FALSE(std::get<bool>(*lab.store.value(clock, "tick")));  // der erste Blick ist kein Tick
    lab.tempo->update(0.25);
    lab.run(0.25);
    CHECK(lab.number(clock, "phase") == Approx(0.5));
    lab.tempo->update(0.5);
    lab.run(0.5);
    CHECK(std::get<bool>(*lab.store.value(clock, "tick")));
    CHECK(std::get<std::int64_t>(*lab.store.value(clock, "count")) == 1);

    // Ein zweiter Clock (z. B. in einem später geladenen Look) steht sofort an derselben Stelle
    const NodeId other = lab.add("wh.value.clock");
    lab.set(other, "globalTempo", true);
    lab.set(other, "ratio", std::int64_t{4});
    lab.feed(other, "phase");
    lab.tempo->update(1.6);
    lab.run(1.6);
    CHECK(lab.number(other, "phase") == Approx(lab.number(clock, "phase")));
    CHECK(std::get<std::int64_t>(*lab.store.value(other, "count")) == 3);

    // Running aus hält an
    lab.set(clock, "running", false);
    lab.tempo->update(2.6);
    lab.run(2.6);
    CHECK(std::get<std::int64_t>(*lab.store.value(clock, "count")) == 3);
    CHECK_FALSE(std::get<bool>(*lab.store.value(clock, "tick")));
}

TEST_CASE("Globales Tempo: Beat liefert die gemeinsame Uhr, Shot nimmt ihre Schlaglänge", "[nodes][tempo]") {
    Lab lab;
    lab.tempo->setFixedBpm(120.0);
    lab.tempo->setSource(values::TempoClock::Source::Fixed);
    const NodeId beat = lab.add("wh.value.beat");
    lab.set(beat, "globalTempo", true);
    lab.feed(beat, "phase");
    const NodeId shot = lab.add("wh.value.shot");
    lab.set(shot, "globalTempo", true);
    lab.set(shot, "bpm", 30.0);  // zählt mit globalem Tempo nicht
    lab.feed(shot, "progress");
    lab.tempo->update(0.0);
    lab.run(0.0);
    lab.tempo->update(0.125);
    lab.run(0.125, 0.125);
    CHECK(lab.number(beat, "bpm") == Approx(120.0));
    CHECK(lab.number(beat, "phase") == Approx(0.25));
    CHECK(std::get<bool>(*lab.store.value(beat, "present")));

    lab.set(shot, "trigger", true);  // ein Durchlauf = 1 Beat = 0,5 s bei 120 BPM
    lab.tempo->update(0.25);
    lab.run(0.25, 0.125);
    lab.tempo->update(0.5);
    lab.run(0.5, 0.25);
    CHECK(lab.number(shot, "progress") == Approx(0.5));

    // Ohne die Option bleibt alles wie bisher: der Shot nimmt `bpm`
    lab.set(shot, "globalTempo", false);
    lab.set(shot, "trigger", false);
    lab.run(0.6, 0.1);
    lab.set(shot, "trigger", true);
    lab.run(0.7, 0.1);
    lab.run(1.2, 0.5);
    CHECK(lab.number(shot, "progress") == Approx(0.25));  // 0,5 s von 2 s (30 BPM)
}
