// perf_bench_test.cpp: grobe Zeitmessung der Teile, die pro Frame oder pro Änderung laufen.
//
// Warum es die Datei gibt: "Performance" ohne Zahlen ist Raten. Diese Messungen sind versteckt (Tag [.bench]) und
// laufen nur auf Zuruf: wh_tests "[bench]" (am besten in einem Release-Build). Sie prüfen keine Grenzwerte, sondern
// drucken Mikrosekunden pro Aufruf, damit Änderungen verglichen werden können.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>

#include "builtin_lab.hpp"

using namespace whitehole;
using whitehole::test::Lab;

namespace {

template <typename F>
double microseconds(int runs, F&& body) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < runs; ++i) {
        body();
    }
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count() / runs;
}

}  // namespace

TEST_CASE("Messung: Feldgraph übersetzen (zwölf Nodes)", "[.bench][bench]") {
    Lab lab;
    NodeId last = lab.add("wh.generator.plasma");
    for (int i = 0; i < 3; ++i) {
        const NodeId rotate = lab.add("wh.transform.rotate");
        lab.connect(last, "out", rotate, "source");
        const NodeId mirror = lab.add("wh.transform.mirror");
        lab.connect(rotate, "out", mirror, "source");
        const NodeId adjust = lab.add("wh.modifier.coloradjust");
        lab.connect(mirror, "out", adjust, "source");
        last = adjust;
    }
    const graph::PortRef target{last, "out"};
    REQUIRE(compiler::compileField(lab.graph, lab.snippets, target).ok());
    const double us = microseconds(500, [&] { (void)compiler::compileField(lab.graph, lab.snippets, target); });
    std::printf("compileField, 10 Nodes: %.1f us pro Aufruf (%.2f ms)\n", us, us / 1000.0);
}

TEST_CASE("Messung: Werte-Graph auswerten (Clock, Sequencer, Envelope, Math-Kette)", "[.bench][bench]") {
    Lab lab;
    const NodeId clock = lab.add("wh.value.clock");
    const NodeId seq = lab.add("wh.value.sequencer");
    const NodeId env = lab.add("wh.value.envelope");
    lab.connect(clock, "tick", seq, "advance");
    lab.connect(seq, "hit", env, "gate");
    NodeId value = env;
    const char* port = "out";
    for (int i = 0; i < 20; ++i) {
        const NodeId math = lab.add("wh.value.math");
        lab.connect(value, port, math, "a");
        value = math;
        port = "out";
    }
    lab.feed(value, port);
    double t = 0.0;
    std::uint64_t frame = 0;
    const double us = microseconds(20000, [&] {
        t += 1.0 / 40.0;
        (void)lab.evaluator.evaluate(lab.graph, FrameTime{frame++, Seconds{t}, Seconds{1.0 / 40.0}}, lab.store);
    });
    std::printf("ValueEvaluator, 25 Nodes: %.2f us pro Frame\n", us);
}
