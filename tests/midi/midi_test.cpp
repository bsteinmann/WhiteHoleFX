// MIDI-Kern: Nachrichten lesen/schreiben, MidiStore (Zustand, Ereignisse, Alter), Portsuche, MidiReceiver mit Fake-Gerät.
// Warum: Alles, was auf Hardware aufsetzt (Nodes, APC-Profil), verlässt sich darauf; ein Fehler hier wäre am Gerät
// nur schwer von einem Verdrahtungsfehler zu unterscheiden.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <memory>

#include "fake_midi.hpp"
#include "whitehole/midi/midi_message.hpp"
#include "whitehole/midi/midi_receiver.hpp"
#include "whitehole/midi/midi_store.hpp"

using namespace whitehole::midi;
using whitehole::ErrorCode;
using whitehole::midi::testing::FakeMidiInput;
using whitehole::midi::testing::FakeMidiOutput;

TEST_CASE("MIDI: parseMidi liest Note an/aus und Regler", "[midi]") {
    const std::array<std::uint8_t, 3> on{0x93, 60, 100};
    const auto message = parseMidi(on);
    REQUIRE(message.has_value());
    CHECK(*message == noteOn(3, 60, 100));

    const std::array<std::uint8_t, 3> off{0x80, 60, 40};
    CHECK(parseMidi(off)->kind == MessageKind::NoteOff);

    const std::array<std::uint8_t, 3> cc{0xB0, 48, 127};
    CHECK(*parseMidi(cc) == controlChange(0, 48, 127));
}

TEST_CASE("MIDI: Note-On mit Velocity 0 gilt als Note-Off", "[midi]") {
    const std::array<std::uint8_t, 3> bytes{0x90, 10, 0};
    CHECK(parseMidi(bytes)->kind == MessageKind::NoteOff);
}

TEST_CASE("MIDI: parseMidi lehnt Unbekanntes, Kurzes und Ungültiges ab", "[midi]") {
    const std::array<std::uint8_t, 2> shortMessage{0x90, 60};
    CHECK_FALSE(parseMidi(shortMessage).has_value());
    const std::array<std::uint8_t, 3> clock{0xF8, 0, 0};
    CHECK_FALSE(parseMidi(clock).has_value());
    const std::array<std::uint8_t, 3> program{0xC0, 5, 0};
    CHECK_FALSE(parseMidi(program).has_value());
    const std::array<std::uint8_t, 3> bigData{0x90, 200, 10};
    CHECK_FALSE(parseMidi(bigData).has_value());
}

TEST_CASE("MIDI: toBytes ist die Umkehrung von parseMidi", "[midi]") {
    for (const MidiMessage& original : {noteOn(15, 127, 1), noteOff(2, 0), controlChange(9, 56, 77)}) {
        const MidiBytes bytes = toBytes(original);
        const std::array<std::uint8_t, 3> raw{bytes.status, bytes.data1, bytes.data2};
        const auto parsed = parseMidi(raw);
        REQUIRE(parsed.has_value());
        // noteOff schreibt Velocity 0 als 0x80; das Wiederlesen liefert dieselbe Nachricht.
        CHECK(*parsed == original);
    }
}

TEST_CASE("MidiStore: Regler und Noten merken ihren letzten Stand", "[midi]") {
    MidiStore store;
    CHECK_FALSE(store.readCc(0, 48).has_value());
    CHECK_FALSE(store.readNote(0, 10).has_value());
    CHECK_FALSE(store.last().has_value());

    store.apply(controlChange(0, 48, 64));
    store.apply(controlChange(0, 48, 99));
    store.apply(noteOn(0, 10, 90));
    REQUIRE(store.readCc(0, 48).has_value());
    CHECK(store.readCc(0, 48)->value == 99);
    CHECK_FALSE(store.readCc(1, 48).has_value());  // anderer Kanal
    CHECK(store.readNote(0, 10)->on);
    CHECK(store.readNote(0, 10)->velocity == 90);

    store.apply(noteOff(0, 10));
    CHECK_FALSE(store.readNote(0, 10)->on);
    CHECK(store.readNote(0, 10)->velocity == 90);  // Velocity des letzten Note-On bleibt
    CHECK(store.last()->message == noteOff(0, 10));
    CHECK(store.stats().messages == 4);
}

TEST_CASE("MidiStore: Alter folgt der eigenen Uhr", "[midi]") {
    auto now = std::chrono::steady_clock::now();
    MidiStore store{[&now] { return now; }};
    store.apply(controlChange(0, 1, 5));
    now += std::chrono::milliseconds(1500);
    CHECK(store.readCc(0, 1)->ageSeconds == Catch::Approx(1.5));
    CHECK(store.last()->ageSeconds == Catch::Approx(1.5));
}

TEST_CASE("MidiStore: takeEvents liefert jede Nachricht genau einmal, in Reihenfolge", "[midi]") {
    MidiStore store;
    store.apply(noteOn(0, 1, 100));
    store.apply(noteOff(0, 1));
    const auto first = store.takeEvents();
    REQUIRE(first.size() == 2);
    CHECK(first[0] == noteOn(0, 1, 100));
    CHECK(first[1].kind == MessageKind::NoteOff);
    CHECK(store.takeEvents().empty());
}

TEST_CASE("MidiStore: die Ereignisliste ist begrenzt, die ältesten fallen heraus", "[midi]") {
    MidiStore store;
    const std::size_t extra = 10;
    for (std::size_t i = 0; i < MidiStore::kMaxQueuedEvents + extra; ++i) {
        store.apply(controlChange(0, 1, static_cast<std::uint8_t>(i % 128)));
    }
    const auto events = store.takeEvents();
    CHECK(events.size() == MidiStore::kMaxQueuedEvents);
    CHECK(store.stats().dropped == extra);
    CHECK(events.front().data2 == static_cast<std::uint8_t>(extra % 128));  // die ersten zehn sind weg
}

TEST_CASE("findMidiPort: gleicher Name, dann Name ohne Portnummer, sonst -1", "[midi]") {
    const std::vector<std::string> ports{"Midi Through 14:0", "APC MINI 20:0", "APC MINI 20:1"};
    CHECK(findMidiPort(ports, "APC MINI 20:1") == 2);
    CHECK(findMidiPort(ports, "APC MINI 24:0") == 1);  // neue Nummer nach dem Anstecken, erster passender Port
    CHECK(findMidiPort(ports, "Midi Through") == 0);
    CHECK(findMidiPort(ports, "Irgendwas 1:0") == -1);
    CHECK(findMidiPort(ports, "") == -1);
    CHECK(findMidiPort({}, "APC MINI 20:0") == -1);
}

TEST_CASE("findMidiPort: Windows-Namen mit Nummer bleiben unangetastet", "[midi]") {
    const std::vector<std::string> ports{"APC MINI 1", "APC MINI 2"};
    CHECK(findMidiPort(ports, "APC MINI 2") == 1);
    CHECK(findMidiPort(ports, "APC MINI 3") == -1);
}

TEST_CASE("MidiReceiver: Nachrichten vom Gerät landen im Store", "[midi]") {
    auto input = std::make_unique<FakeMidiInput>();
    FakeMidiInput* device = input.get();
    auto store = std::make_shared<MidiStore>();
    MidiReceiver receiver{std::move(input), store};

    CHECK_FALSE(receiver.running());
    REQUIRE(receiver.start("Fake Eingang 20:0").ok());
    CHECK(receiver.running());
    device->inject(controlChange(0, 56, 33));
    CHECK(store->readCc(0, 56)->value == 33);

    receiver.stop();
    CHECK_FALSE(receiver.running());
    device->inject(controlChange(0, 56, 99));  // ohne offenen Port wirkungslos
    CHECK(store->readCc(0, 56)->value == 33);
}

TEST_CASE("MidiReceiver: leerer Name schaltet ab, unbekannter Port ist ein Fehler", "[midi]") {
    auto store = std::make_shared<MidiStore>();
    MidiReceiver receiver{std::make_unique<FakeMidiInput>(), store};
    REQUIRE(receiver.start("Fake Eingang 20:0").ok());
    REQUIRE(receiver.start("").ok());
    CHECK_FALSE(receiver.running());

    const auto result = receiver.start("Gibt es nicht");
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::NotFound);
    CHECK_FALSE(receiver.running());
}

TEST_CASE("FakeMidiOutput: sendet nur, wenn geöffnet", "[midi]") {
    FakeMidiOutput output;
    CHECK_FALSE(output.send(noteOn(0, 1, 3)).ok());
    REQUIRE(output.open("Fake Ausgang 20:1").ok());
    REQUIRE(output.send(noteOn(0, 1, 3)).ok());
    CHECK(output.sent.size() == 1);
}

TEST_CASE("MIDI: der echte Backend-Eintrag lässt sich ohne Gerät erzeugen", "[midi]") {
    // Je nach Build echtes RtMidi oder Stub; beide müssen ohne Absturz Ports listen und einen unbekannten Port ablehnen.
    const auto input = makeMidiInput();
    const auto output = makeMidiOutput();
    (void)input->ports();
    (void)output->ports();
    CHECK_FALSE(input->open("Gibt es garantiert nicht 99:9", [](const MidiMessage&) {}).ok());
    CHECK_FALSE(output->open("Gibt es garantiert nicht 99:9").ok());
    CHECK_FALSE(input->isOpen());
    CHECK_FALSE(output->isOpen());
}
