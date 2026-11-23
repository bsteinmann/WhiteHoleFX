// MidiLearn: die nächste passende Nachricht als Zuweisung übernehmen (ohne die Ereignisliste anzutasten).
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "whitehole/midi/midi_learn.hpp"

using namespace whitehole::midi;

TEST_CASE("Lernen: Controller - nur eine neue Reglernachricht zählt", "[midi][learn]") {
    auto store = std::make_shared<MidiStore>();
    MidiLearn learn{store};
    CHECK_FALSE(learn.armed());
    CHECK_FALSE(learn.poll().has_value());

    store->apply(controlChange(3, 20, 5));  // schon vor dem Scharfstellen da
    learn.arm(MidiLearn::Want::Controller);
    CHECK(learn.armed());
    CHECK_FALSE(learn.poll().has_value());

    store->apply(noteOn(0, 60, 100));  // falsche Art
    CHECK_FALSE(learn.poll().has_value());

    store->apply(controlChange(1, 49, 64));
    store->apply(noteOff(0, 60));  // verdeckt die Reglernachricht nicht
    const auto got = learn.poll();
    REQUIRE(got.has_value());
    CHECK(got->kind == MessageKind::ControlChange);
    CHECK(got->channel == 1);
    CHECK(got->data1 == 49);
    CHECK_FALSE(learn.armed());  // einmalig
    CHECK_FALSE(learn.poll().has_value());
}

TEST_CASE("Lernen: Note - ein Note-Off danach verdeckt den Anschlag nicht", "[midi][learn]") {
    auto store = std::make_shared<MidiStore>();
    MidiLearn learn{store};
    learn.arm(MidiLearn::Want::Note);
    store->apply(controlChange(0, 1, 1));  // falsche Art
    CHECK_FALSE(learn.poll().has_value());
    store->apply(noteOn(2, 36, 90));
    store->apply(noteOff(2, 36));
    const auto got = learn.poll();
    REQUIRE(got.has_value());
    CHECK(got->kind == MessageKind::NoteOn);
    CHECK(got->channel == 2);
    CHECK(got->data1 == 36);
}

TEST_CASE("Lernen: abbrechen und neu scharfstellen", "[midi][learn]") {
    auto store = std::make_shared<MidiStore>();
    MidiLearn learn{store};
    learn.arm(MidiLearn::Want::Controller);
    learn.cancel();
    store->apply(controlChange(0, 1, 1));
    CHECK_FALSE(learn.poll().has_value());
    learn.arm(MidiLearn::Want::Controller);  // die alte Nachricht zählt nicht mehr
    CHECK_FALSE(learn.poll().has_value());
    store->apply(controlChange(0, 2, 1));
    CHECK(learn.poll().has_value());
}

TEST_CASE("MidiStore: laufende Nummern und letzte Nachricht je Art", "[midi][store]") {
    MidiStore store;
    CHECK_FALSE(store.lastControlChange().has_value());
    store.apply(controlChange(0, 1, 2));
    store.apply(noteOn(0, 3, 4));
    store.apply(noteOff(0, 3));
    CHECK(store.lastControlChange()->sequence == 1);
    CHECK(store.lastNoteOn()->sequence == 2);
    CHECK(store.last()->sequence == 3);
}
