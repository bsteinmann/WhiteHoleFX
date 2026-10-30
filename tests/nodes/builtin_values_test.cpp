// Die Werte-Nodes (Time, Number, Color, LFO, Random, Map, Smooth) einzeln.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <set>
#include <variant>

#include "builtin_lab.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

TEST_CASE("Time: Tempo und Versatz") {
    Lab lab;
    const NodeId time = lab.add("wh.value.time");
    lab.feed(time, "seconds");

    CHECK(lab.sample(time, 3.0, "seconds") == Approx(3.0));

    lab.set(time, "speed", 2.0);
    lab.set(time, "offset", 1.0);
    CHECK(lab.sample(time, 3.0, "seconds") == Approx(7.0));

    lab.set(time, "speed", -1.0);
    lab.set(time, "offset", 0.0);
    CHECK(lab.sample(time, 3.0, "seconds") == Approx(-3.0));

    lab.set(time, "speed", 5000.0);  // wird auf 1000 begrenzt
    CHECK(lab.sample(time, 3.0, "seconds") == Approx(3000.0));
}

TEST_CASE("Number und Color reichen ihren Parameter durch") {
    Lab lab;
    const NodeId number = lab.add("wh.value.number");
    const NodeId color = lab.add("wh.value.color");
    lab.feed(number, "out");
    const NodeId gradient = lab.add("wh.generator.gradient");
    lab.connect(color, "out", gradient, "colorA");

    lab.set(number, "value", -2.5);
    lab.set(color, "color", ColorRgba{0.1F, 0.2F, 0.3F, 0.4F});
    lab.run(0.0);

    CHECK(lab.number(number, "out") == Approx(-2.5));
    const auto stored = lab.store.value(color, "out");
    REQUIRE(stored.has_value());
    CHECK(std::get<ColorRgba>(*stored) == ColorRgba{0.1F, 0.2F, 0.3F, 0.4F});
}

TEST_CASE("LFO: Wellenformen an bekannten Phasen") {
    Lab lab;
    const NodeId lfo = lab.add("wh.value.lfo");
    lab.feed(lfo, "out");
    lab.set(lfo, "frequency", 0.5);  // eine Periode dauert 2 s: t = 0.5 s entspricht u = 0.25

    SECTION("Sinus") {
        lab.set(lfo, "shape", std::int64_t{0});
        CHECK(lab.sample(lfo, 0.0) == Approx(0.5));
        CHECK(lab.sample(lfo, 0.5) == Approx(1.0));
        CHECK(lab.sample(lfo, 1.0) == Approx(0.5).margin(1e-12));
        CHECK(lab.sample(lfo, 1.5) == Approx(0.0).margin(1e-12));
    }
    SECTION("Dreieck") {
        lab.set(lfo, "shape", std::int64_t{1});
        CHECK(lab.sample(lfo, 0.0) == Approx(0.0).margin(1e-12));
        CHECK(lab.sample(lfo, 0.5) == Approx(0.5));
        CHECK(lab.sample(lfo, 1.0) == Approx(1.0));
        CHECK(lab.sample(lfo, 1.5) == Approx(0.5));
    }
    SECTION("Sägezahn") {
        lab.set(lfo, "shape", std::int64_t{2});
        CHECK(lab.sample(lfo, 0.5) == Approx(0.25));
        CHECK(lab.sample(lfo, 1.5) == Approx(0.75));
        CHECK(lab.sample(lfo, 2.5) == Approx(0.25));  // nächste Periode
    }
    SECTION("Rechteck") {
        lab.set(lfo, "shape", std::int64_t{3});
        CHECK(lab.sample(lfo, 0.5) == Approx(1.0));
        CHECK(lab.sample(lfo, 1.5) == Approx(0.0).margin(1e-12));
    }
    SECTION("ein unzulässiger Formwert wird auf den Bereich begrenzt") {
        lab.set(lfo, "shape", std::int64_t{9});  // -> 3 (Rechteck)
        CHECK(lab.sample(lfo, 0.5) == Approx(1.0));
        CHECK(lab.sample(lfo, 1.5) == Approx(0.0).margin(1e-12));
    }
    SECTION("Bereich und Phase") {
        lab.set(lfo, "shape", std::int64_t{2});
        lab.set(lfo, "min", -1.0);
        lab.set(lfo, "max", 3.0);
        CHECK(lab.sample(lfo, 1.0) == Approx(1.0));  // u = 0.5 -> -1 + 4 * 0.5
        lab.set(lfo, "phase", 0.5);
        CHECK(lab.sample(lfo, 0.0) == Approx(1.0));  // u = 0.5
    }
}

TEST_CASE("LFO: Frequenz lässt sich von einem anderen Wert steuern") {
    Lab lab;
    const NodeId speed = lab.add("wh.value.number");
    const NodeId lfo = lab.add("wh.value.lfo");
    lab.feed(lfo, "out");
    lab.set(lfo, "shape", std::int64_t{2});
    lab.connect(speed, "out", lfo, "frequency");

    lab.set(speed, "value", 1.0);
    CHECK(lab.sample(lfo, 0.25) == Approx(0.25));
    lab.set(speed, "value", 2.0);
    CHECK(lab.sample(lfo, 0.25) == Approx(0.5));
    lab.set(speed, "value", 500.0);  // über dem Bereich des Parameters: auf 100 Hz begrenzt
    CHECK(lab.sample(lfo, 0.0025) == Approx(0.25));
}

TEST_CASE("Random: Wertebereich, Reproduzierbarkeit, Seed und Rate") {
    Lab a;
    Lab b;
    const NodeId ra = a.add("wh.value.random");
    const NodeId rb = b.add("wh.value.random");
    a.feed(ra, "out");
    b.feed(rb, "out");

    std::set<double> distinct;
    for (int i = 0; i < 2000; ++i) {
        const double t = i * 0.037;
        const double va = a.sample(ra, t);
        REQUIRE(va >= 0.0);
        REQUIRE(va < 1.0);
        CHECK(va == b.sample(rb, t));  // gleiche Eingabe, gleicher Wert
        distinct.insert(va);
    }
    CHECK(distinct.size() > 1500U);

    SECTION("anderer Seed, andere Folge") {
        const double before = a.sample(ra, 5.5);
        a.set(ra, "seed", std::int64_t{1});
        CHECK(a.sample(ra, 5.5) != before);
    }
    SECTION("doppelte Rate entspricht doppelter Zeit") {
        const double slow = a.sample(ra, 3.2);
        a.set(ra, "rate", 2.0);
        CHECK(a.sample(ra, 1.6) == Approx(slow));
    }
}

TEST_CASE("Random: gehalten oder weich") {
    Lab lab;
    const NodeId random = lab.add("wh.value.random");
    lab.feed(random, "out");

    lab.set(random, "smooth", false);
    std::set<double> cells;
    for (int cell = 0; cell < 20; ++cell) {
        const double early = lab.sample(random, cell + 0.1);
        const double late = lab.sample(random, cell + 0.9);
        CHECK(early == late);  // innerhalb einer Zelle konstant
        cells.insert(early);
    }
    CHECK(cells.size() >= 18U);  // und von Zelle zu Zelle verschieden

    const double held = lab.sample(random, 2.0);
    lab.set(random, "smooth", true);
    CHECK(lab.sample(random, 2.0) == held);                                     // an den Stützstellen gleich
    CHECK(lab.sample(random, 2.999999) == Approx(lab.sample(random, 3.0)).margin(1e-3));  // stetig
}

TEST_CASE("Map: Bereich umrechnen") {
    Lab lab;
    const NodeId map = lab.add("wh.value.map");
    lab.feed(map, "out");
    lab.set(map, "inMax", 10.0);
    lab.set(map, "outMax", 100.0);

    lab.set(map, "value", 5.0);
    CHECK(lab.sample(map, 0.0) == Approx(50.0));

    SECTION("begrenzt") {
        lab.set(map, "value", 20.0);
        CHECK(lab.sample(map, 0.0) == Approx(100.0));
        lab.set(map, "value", -5.0);
        CHECK(lab.sample(map, 0.0) == Approx(0.0));
    }
    SECTION("ohne Begrenzung") {
        lab.set(map, "clamp", false);
        lab.set(map, "value", 20.0);
        CHECK(lab.sample(map, 0.0) == Approx(200.0));
    }
    SECTION("umgekehrter Ausgangsbereich") {
        lab.set(map, "outMin", 100.0);
        lab.set(map, "outMax", 0.0);
        lab.set(map, "value", 2.5);
        CHECK(lab.sample(map, 0.0) == Approx(75.0));
    }
    SECTION("leerer Eingangsbereich liefert outMin") {
        lab.set(map, "inMin", 1.0);
        lab.set(map, "inMax", 1.0);
        lab.set(map, "outMin", 7.0);
        CHECK(lab.sample(map, 0.0) == Approx(7.0));
    }
    SECTION("der Eingang kann von einem anderen Node kommen") {
        const NodeId time = lab.add("wh.value.time");
        lab.connect(time, "seconds", map, "value");
        CHECK(lab.sample(map, 2.0) == Approx(20.0));
    }
}

TEST_CASE("Smooth: Tiefpass mit Zustand") {
    Lab lab;
    const NodeId target = lab.add("wh.value.number");
    const NodeId smooth = lab.add("wh.value.smooth");
    lab.feed(smooth, "out");
    lab.connect(target, "out", smooth, "value");
    lab.set(smooth, "time", 1.0);
    lab.set(target, "value", 1.0);

    // Der erste Frame übernimmt das Ziel ohne Anlauf von null.
    lab.run(0.0, 0.0);
    CHECK(lab.number(smooth, "out") == Approx(1.0));

    // Sprung auf 0: Nach einer Zeitkonstante bleibt 1/e übrig, nach zwei 1/e^2.
    lab.set(target, "value", 0.0);
    lab.run(1.0, 1.0);
    CHECK(lab.number(smooth, "out") == Approx(std::exp(-1.0)));
    lab.run(2.0, 1.0);
    CHECK(lab.number(smooth, "out") == Approx(std::exp(-2.0)));

    SECTION("Zeitkonstante 0 folgt sofort") {
        lab.set(smooth, "time", 0.0);
        lab.set(target, "value", 5.0);
        lab.run(3.0, 0.016);
        CHECK(lab.number(smooth, "out") == Approx(5.0));
    }
    SECTION("ein sehr langer Frame schießt nicht über das Ziel hinaus") {
        lab.set(target, "value", 10.0);
        lab.run(1000.0, 1000.0);
        CHECK(lab.number(smooth, "out") == Approx(10.0));
    }
}
