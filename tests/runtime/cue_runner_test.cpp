// Tests des Cuelist-Ablaufs (Issue #21): Auslöser, Delay und Fade, Flanken, Tasten, GO.
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

#include "whitehole/runtime/cue_runner.hpp"

using namespace whitehole;
using project::Cue;
using project::CueTrigger;
using runtime::CueEvents;
using runtime::CueRunner;

namespace {

Cue cue(CueTrigger trigger, double delay = 0.0, double fade = 1.0) {
    Cue c;
    c.look = "L";
    c.trigger = trigger;
    c.delay = delay;
    c.fade = fade;
    return c;
}

CueEvents none() { return {}; }

CueEvents go() {
    CueEvents e;
    e.go = true;
    return e;
}

}  // namespace

TEST_CASE("Cuelist: GO schaltet Cue für Cue weiter, am Ende ist Schluss", "[cue_runner]") {
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), cue(CueTrigger::Go), cue(CueTrigger::Go)});
    CHECK_FALSE(runner.running());
    CHECK_FALSE(runner.update(0.0, go()).has_value());  // angehalten: nichts

    REQUIRE(runner.start(0.0));
    CHECK(runner.status(0.0).current == 0);
    CHECK(runner.status(0.0).next == 1);
    CHECK_FALSE(runner.update(0.1, none()).has_value());  // GO-Cue wartet

    CHECK(runner.update(1.0, go()) == 1);
    CHECK(runner.status(1.0).current == 1);
    CHECK(runner.update(2.0, go()) == 2);
    CHECK(runner.status(2.0).next == -1);
    CHECK_FALSE(runner.update(3.0, go()).has_value());  // nach dem letzten Cue kommt nichts mehr
}

TEST_CASE("Cuelist: Delay verzögert den Beginn der Überblendung", "[cue_runner]") {
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), cue(CueTrigger::Go, 2.0, 3.0)});
    runner.start(0.0);
    CHECK_FALSE(runner.update(10.0, go()).has_value());  // ausgelöst, Delay läuft
    CHECK(runner.status(10.0).pending == 1);
    CHECK(runner.status(10.5).pendingSeconds == 1.5);
    CHECK_FALSE(runner.update(11.9, none()).has_value());
    CHECK(runner.update(12.0, none()) == 1);              // jetzt beginnt die Überblendung
    CHECK(runner.status(12.0).pending == -1);
    CHECK(runner.status(13.5).fading);
    CHECK(runner.status(13.5).fadeProgress == 0.5);
    CHECK_FALSE(runner.status(15.5).fading);
}

TEST_CASE("Cuelist: Time folgt nach dem Ende der vorigen Überblendung", "[cue_runner]") {
    Cue timed = cue(CueTrigger::Time, 0.0, 1.0);
    timed.time = 4.0;
    CueRunner runner;
    Cue first = cue(CueTrigger::Go, 0.0, 0.0);
    runner.load({first, cue(CueTrigger::Go, 0.0, 2.0), timed});
    runner.start(0.0);
    CHECK(runner.update(5.0, go()) == 1);                 // Überblendung 5..7
    CHECK_FALSE(runner.update(10.9, none()).has_value());  // 7 + 4 = 11
    CHECK(runner.update(11.0, none()) == 2);
}

TEST_CASE("Cuelist: Time mit 0 Sekunden läuft hintereinander durch", "[cue_runner]") {
    CueRunner runner;
    Cue a = cue(CueTrigger::Time, 0.0, 0.0);
    a.time = 0.0;
    runner.load({cue(CueTrigger::Go), a, a, a});
    runner.start(0.0);
    CHECK(runner.update(0.0, none()) == 1);  // ein Cue je Aufruf
    CHECK(runner.update(0.1, none()) == 2);
    CHECK(runner.update(0.2, none()) == 3);
    CHECK_FALSE(runner.update(0.3, none()).has_value());
}

TEST_CASE("Cuelist: Tasten springen zu ihrem Cue, auch rückwärts, Groß/Klein egal", "[cue_runner]") {
    Cue k1 = cue(CueTrigger::Key);
    k1.key = "F5";
    Cue k2 = cue(CueTrigger::Key);
    k2.key = "a";
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), k1, cue(CueTrigger::Go), k2});
    runner.start(0.0);
    CueEvents e;
    e.keys = {"A"};
    CHECK(runner.update(1.0, e) == 3);  // vor und über Cue 1 und 2 hinweg
    CHECK(runner.status(1.0).next == -1);
    e.keys = {"f5"};
    CHECK(runner.update(2.0, e) == 1);  // zurück
    CHECK(runner.status(2.0).next == 2);
    e.keys = {"X"};  // unbekannte Taste
    CHECK_FALSE(runner.update(3.0, e).has_value());
}

TEST_CASE("Cuelist: DMX löst bei einem Anstieg über die Schwelle aus, nicht bei schon hohem Wert", "[cue_runner]") {
    Cue d = cue(CueTrigger::Dmx);
    d.dmxThreshold = 100;
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), d});
    runner.start(0.0);
    int value = 200;  // der Regler steht schon oben, als der Cue an die Reihe kommt
    CueEvents e;
    e.dmx = [&](const Cue&) -> std::optional<int> { return value; };
    CHECK_FALSE(runner.update(0.1, e).has_value());
    CHECK_FALSE(runner.update(0.2, e).has_value());
    value = 0;
    CHECK_FALSE(runner.update(0.3, e).has_value());
    value = 99;
    CHECK_FALSE(runner.update(0.4, e).has_value());
    value = 100;
    CHECK(runner.update(0.5, e) == 1);
}

TEST_CASE("Cuelist: DMX ohne Signal löst nicht aus", "[cue_runner]") {
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), cue(CueTrigger::Dmx)});
    runner.start(0.0);
    CueEvents e;
    e.dmx = [](const Cue&) -> std::optional<int> { return std::nullopt; };
    CHECK_FALSE(runner.update(0.1, e).has_value());
    CHECK_FALSE(runner.update(0.2, e).has_value());
    CHECK(runner.update(0.3, go()) == 1);  // GO geht immer
}

TEST_CASE("Cuelist: Audio löst beim Beat aus", "[cue_runner]") {
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), cue(CueTrigger::Audio), cue(CueTrigger::Audio)});
    runner.start(0.0);
    CHECK_FALSE(runner.update(0.1, none()).has_value());
    CueEvents beat;
    beat.beat = true;
    CHECK(runner.update(0.2, beat) == 1);
    CHECK(runner.update(0.3, beat) == 2);
}

TEST_CASE("Cuelist: fire springt zu einem Cue, ein neuer Auslöser ersetzt den wartenden", "[cue_runner]") {
    CueRunner runner;
    runner.load({cue(CueTrigger::Go), cue(CueTrigger::Go, 5.0), cue(CueTrigger::Go, 0.0), cue(CueTrigger::Go)});
    CHECK_FALSE(runner.fire(1, 0.0));  // angehalten
    runner.start(0.0);
    CHECK_FALSE(runner.fire(4, 0.0));
    CHECK_FALSE(runner.fire(-1, 0.0));
    CHECK(runner.fire(1, 1.0));  // wartet 5 s
    CHECK(runner.fire(2, 2.0));  // ersetzt ihn, ohne Delay
    CHECK(runner.update(2.0, none()) == 2);
    CHECK_FALSE(runner.update(7.0, none()).has_value());  // Cue 1 wurde ersetzt und kommt nicht mehr
    CHECK(runner.status(7.0).next == 3);
}

TEST_CASE("Cuelist: start ohne Cues und stop", "[cue_runner]") {
    CueRunner runner;
    CHECK_FALSE(runner.start(0.0));
    runner.load({cue(CueTrigger::Go), cue(CueTrigger::Go)});
    REQUIRE(runner.start(0.0));
    runner.stop();
    CHECK_FALSE(runner.running());
    CHECK(runner.status(0.0).current == -1);
    CHECK_FALSE(runner.update(1.0, go()).has_value());
}
