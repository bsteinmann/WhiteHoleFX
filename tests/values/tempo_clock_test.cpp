// Gemeinsame Tempo-Uhr (values::TempoClock): stetige Phase, Tap zieht Beat und Tempo nach, Einrasten gleicht sich an,
// fester Wert. Die Zeit kommt als Zahl, daher genau prüfbar.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "whitehole/values/tempo_clock.hpp"

using whitehole::values::TempoClock;
using Catch::Approx;

TEST_CASE("Tempo-Uhr: läuft mit 120 BPM und meldet jeden Beat genau einmal", "[values][tempo]") {
    TempoClock clock;
    clock.update(10.0);
    CHECK(clock.snapshot().beatNow);  // Beat 0 beginnt
    CHECK(clock.snapshot().bpm == Approx(120.0));
    clock.update(10.25);
    CHECK_FALSE(clock.snapshot().beatNow);
    CHECK(clock.snapshot().phase == Approx(0.5));
    clock.update(10.5);
    CHECK(clock.snapshot().beatNow);
    CHECK(clock.snapshot().beat == 1);
    CHECK(clock.snapshot().sinceBeat == Approx(0.0));
    clock.update(10.5);  // dasselbe Bild noch einmal: kein zweiter Beat
    CHECK_FALSE(clock.snapshot().beatNow);
    clock.update(9.0);  // rückwärts zählt als Stillstand
    CHECK(clock.snapshot().beat == 1);
    CHECK(clock.snapshot().beats == Approx(1.0));
}

TEST_CASE("Tempo-Uhr: Taps setzen das Tempo und ziehen den Beat auf den Tap", "[values][tempo]") {
    TempoClock clock;
    clock.update(0.0);
    // Taps alle 0,4 s = 150 BPM
    for (const double t : {0.1, 0.5, 0.9, 1.3}) {
        clock.update(t - 0.01);
        clock.tap();
        clock.update(t);
    }
    CHECK(clock.snapshot().bpm == Approx(150.0));
    // Nach dem Angleichen liegt jeder weitere Beat 0,4 s nach dem letzten Tap
    clock.update(1.3 + TempoClock::kSettleSeconds + 0.01);
    const std::int64_t beatAtTap = clock.snapshot().beat;
    clock.update(1.69);
    CHECK(clock.snapshot().beat == beatAtTap);
    clock.update(1.71);
    CHECK(clock.snapshot().beat == beatAtTap + 1);
    CHECK(clock.snapshot().beatNow);

    // Eine lange Pause beginnt eine neue Folge: das Tempo bleibt, der Beat rastet ein
    clock.update(10.0);
    clock.tap();
    clock.update(10.0);
    CHECK(clock.snapshot().bpm == Approx(150.0));

    // Bei fester Quelle zählt ein Tap nicht
    clock.setSource(TempoClock::Source::Fixed);
    clock.tap();
    clock.update(10.1);
    CHECK(clock.snapshot().bpm == Approx(TempoClock::kDefaultBpm));
}

TEST_CASE("Tempo-Uhr: ein Tap kurz vor dem Beat löst ihn sofort aus, kurz danach keinen zweiten", "[values][tempo]") {
    TempoClock early;
    early.update(0.0);
    early.update(0.45);  // 120 BPM: Beat 1 käme bei 0,5 s
    early.tap();
    early.update(0.45);
    CHECK(early.snapshot().beatNow);
    CHECK(early.snapshot().beat == 1);
    early.update(0.5);
    CHECK_FALSE(early.snapshot().beatNow);  // derselbe Beat kommt nicht noch einmal

    TempoClock late;
    late.update(0.0);
    late.update(0.5);
    CHECK(late.snapshot().beatNow);
    late.update(0.55);
    late.tap();
    late.update(0.55);
    CHECK_FALSE(late.snapshot().beatNow);
    CHECK(late.snapshot().sinceBeat == Approx(0.0));  // der Puls zählt ab dem Tap
}

TEST_CASE("Tempo-Uhr: Einrasten gleicht sich in kurzer Zeit an, ohne Sprung", "[values][tempo]") {
    TempoClock clock;
    clock.update(0.0);
    clock.update(0.1);  // Phase 0,2
    const double before = clock.snapshot().beats;
    clock.tap();  // rastet auf Beat 0 ein (kürzester Weg)
    clock.update(0.1);
    CHECK(clock.snapshot().beats == Approx(before));  // im Bild des Taps noch kein Sprung
    double last = clock.snapshot().beats;
    for (double t = 0.12; t < 0.1 + TempoClock::kSettleSeconds; t += 0.02) {
        clock.update(t);
        const double now = clock.snapshot().beats;
        CHECK(std::abs(now - last) < 0.1);  // kleine Schritte statt eines Sprungs um 0,2 Beats
        last = now;
    }
    clock.update(0.1 + TempoClock::kSettleSeconds + 0.001);
    CHECK(clock.snapshot().beats == Approx(0.0 + (TempoClock::kSettleSeconds + 0.001) * 2.0).margin(1e-9));
}

TEST_CASE("Tempo-Uhr: fester Wert ändert das Tempo, die Phase läuft stetig weiter", "[values][tempo]") {
    TempoClock clock;
    clock.update(0.0);
    clock.update(0.25);  // 0,5 Beats
    clock.setFixedBpm(60.0);
    CHECK(clock.snapshot().bpm == Approx(120.0));  // Quelle ist noch Tap
    clock.setSource(TempoClock::Source::Fixed);
    CHECK(clock.snapshot().bpm == Approx(60.0));
    CHECK(clock.snapshot().beats == Approx(0.5));
    clock.update(0.75);  // 0,5 s bei 60 BPM = 0,5 Beats
    CHECK(clock.snapshot().beats == Approx(1.0));
    CHECK(clock.snapshot().beatNow);
    clock.setFixedBpm(1000.0);
    CHECK(clock.snapshot().bpm == Approx(TempoClock::kMaxBpm));
    CHECK(clock.fixedBpm() == Approx(TempoClock::kMaxBpm));
}
