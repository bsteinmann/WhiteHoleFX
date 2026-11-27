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
