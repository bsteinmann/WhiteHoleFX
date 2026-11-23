// Die MIDI-Nodes: "MIDI CC" und "MIDI Note" lesen aus dem gemeinsamen MidiStore.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <variant>

#include "builtin_lab.hpp"
#include "whitehole/midi/midi_message.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

namespace {

bool flag(Lab& lab, NodeId node, const char* port) {
    const auto value = lab.store.value(node, port);
    REQUIRE(value.has_value());
    return std::get<bool>(*value);
}

NodeId ccNode(Lab& lab) {
    const NodeId node = lab.add("wh.value.midicc");
    lab.feed(node, "value");
    const NodeId sink = lab.add("wh.transform.rotate");  // zweiter Verbraucher für die Schalter
    lab.connect(node, "present", sink, "angle");
    return node;
}

NodeId noteNode(Lab& lab) {
    const NodeId node = lab.add("wh.value.midinote");
    lab.feed(node, "velocity");
    const NodeId sink = lab.add("wh.transform.rotate");
    lab.connect(node, "present", sink, "angle");
    return node;
}

}  // namespace

TEST_CASE("MIDI CC: Regler wird auf Min..Max abgebildet", "[midi][nodes]") {
    Lab lab;
    const NodeId node = ccNode(lab);
    lab.set(node, "channel", std::int64_t{2});
    lab.set(node, "cc", std::int64_t{48});
    lab.set(node, "min", -1.0);
    lab.set(node, "max", 1.0);

    CHECK(lab.sample(node, 0.0, "value") == Approx(-1.0));  // noch nie etwas gekommen
    CHECK_FALSE(flag(lab, node, "present"));

    lab.midi->apply(midi::controlChange(1, 48, 127));  // Kanal 2 (0-basiert 1)
    CHECK(lab.sample(node, 0.1, "value") == Approx(1.0));
    CHECK(flag(lab, node, "present"));
    CHECK(flag(lab, node, "on"));

    lab.midi->apply(midi::controlChange(1, 48, 0));
    CHECK(lab.sample(node, 0.2, "value") == Approx(-1.0));
    CHECK_FALSE(flag(lab, node, "on"));
}

TEST_CASE("MIDI CC: andere Regler und Kanäle zählen nicht", "[midi][nodes]") {
    Lab lab;
    const NodeId node = ccNode(lab);  // Kanal 1, CC 0
    lab.midi->apply(midi::controlChange(0, 1, 127));
    lab.midi->apply(midi::controlChange(1, 0, 127));
    CHECK(lab.sample(node, 0.0, "value") == Approx(0.0));
    CHECK_FALSE(flag(lab, node, "present"));
    lab.midi->apply(midi::controlChange(0, 0, 64));
    CHECK(lab.sample(node, 0.1, "value") == Approx(64.0 / 127.0));
}

TEST_CASE("MIDI CC: Signalverlust hält den Wert oder fällt auf 0", "[midi][nodes]") {
    Lab lab;
    const NodeId node = ccNode(lab);
    lab.set(node, "timeout", 1.0);

    lab.midi->apply(midi::controlChange(0, 0, 127));
    CHECK(lab.sample(node, 0.0, "value") == Approx(1.0));
    CHECK(flag(lab, node, "present"));

    lab.midiNow += std::chrono::seconds{2};  // älter als das Zeitlimit
    CHECK(lab.sample(node, 0.1, "value") == Approx(1.0));  // gehalten
    CHECK_FALSE(flag(lab, node, "present"));

    lab.set(node, "hold", false);
    CHECK(lab.sample(node, 0.2, "value") == Approx(0.0));

    lab.midi->apply(midi::controlChange(0, 0, 127));  // Signal kehrt zurück
    CHECK(lab.sample(node, 0.3, "value") == Approx(1.0));

    lab.midiNow += std::chrono::seconds{100};  // Zeitlimit 0 = nie ablaufen (Standard)
    lab.set(node, "timeout", 0.0);
    lab.run(0.4);
    CHECK(flag(lab, node, "present"));
}

TEST_CASE("MIDI Note: Schalter folgt der Taste, Velocity bleibt stehen", "[midi][nodes]") {
    Lab lab;
    const NodeId node = noteNode(lab);
    lab.set(node, "note", std::int64_t{10});

    CHECK(lab.sample(node, 0.0, "velocity") == Approx(0.0));
    CHECK_FALSE(flag(lab, node, "on"));
    CHECK_FALSE(flag(lab, node, "present"));

    lab.midi->apply(midi::noteOn(0, 10, 127));
    CHECK(lab.sample(node, 0.1, "velocity") == Approx(1.0));
    CHECK(flag(lab, node, "on"));
    CHECK(flag(lab, node, "present"));

    lab.midi->apply(midi::noteOff(0, 10));
    CHECK(lab.sample(node, 0.2, "velocity") == Approx(1.0));  // Anschlagstärke bleibt
    CHECK_FALSE(flag(lab, node, "on"));
    CHECK(flag(lab, node, "present"));  // die Taste war da, nur losgelassen

    lab.midi->apply(midi::noteOn(0, 11, 5));  // andere Note: keine Wirkung
    CHECK_FALSE(flag(lab, node, "on"));
}
