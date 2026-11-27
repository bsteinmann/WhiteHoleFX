// MIDI-Clock (Echtzeitmeldungen 0xF8/0xFA/0xFB/0xFC) bis in den MidiStore: Zählen, Start/Stop und Tempo aus den Abständen.
// Für die gemeinsame Tempo-Uhr (Stufe 3, docs/tempo.md).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

#include "fake_midi.hpp"
#include "whitehole/midi/midi_message.hpp"
#include "whitehole/midi/midi_receiver.hpp"
#include "whitehole/midi/midi_store.hpp"

using namespace whitehole;
using Catch::Approx;

TEST_CASE("MIDI-Clock: Echtzeit-Bytes werden erkannt, alles andere nicht", "[midi][tempo]") {
    const std::array<std::uint8_t, 1> clock{0xF8};
    const std::array<std::uint8_t, 1> start{0xFA};
    const std::array<std::uint8_t, 1> cont{0xFB};
    const std::array<std::uint8_t, 1> stop{0xFC};
    const std::array<std::uint8_t, 1> sense{0xFE};
    const std::array<std::uint8_t, 3> note{0x90, 60, 100};
    CHECK(midi::parseMidiRealtime(clock) == midi::MidiRealtime::Clock);
    CHECK(midi::parseMidiRealtime(start) == midi::MidiRealtime::Start);
    CHECK(midi::parseMidiRealtime(cont) == midi::MidiRealtime::Continue);
    CHECK(midi::parseMidiRealtime(stop) == midi::MidiRealtime::Stop);
    CHECK_FALSE(midi::parseMidiRealtime(sense).has_value());
    CHECK_FALSE(midi::parseMidiRealtime(note).has_value());
    CHECK_FALSE(midi::parseMidi(clock).has_value());  // Noten-/Regler-Weg bleibt unberührt
}

TEST_CASE("MIDI-Clock: der Speicher zählt Clocks, Start setzt zurück, das Tempo folgt den Abständen", "[midi][tempo]") {
    auto now = std::chrono::steady_clock::time_point{} + std::chrono::seconds{10};
    auto store = std::make_shared<midi::MidiStore>([&now] { return now; });
    auto input = std::make_unique<midi::testing::FakeMidiInput>();
    const midi::testing::FakeMidiInput* fake = input.get();
    midi::MidiReceiver receiver{std::move(input), store};
    REQUIRE(receiver.start("Fake Eingang 20:0").ok());

    CHECK_FALSE(store->clock().running);
    CHECK(store->clock().ageSeconds > 100.0);
    // 120 BPM: 24 Clocks je halbe Sekunde
    const auto tick = std::chrono::microseconds{500000 / 24};
    fake->injectRealtime(midi::MidiRealtime::Start);
    for (int i = 0; i < 30; ++i) {
        fake->injectRealtime(midi::MidiRealtime::Clock);
        now += tick;
    }
    midi::MidiClockState state = store->clock();
    CHECK(state.running);
    CHECK(state.starts == 1);
    CHECK(state.ticks == 30);
    CHECK(state.bpm == Approx(120.0).epsilon(0.01));
    CHECK(state.ageSeconds == Approx(std::chrono::duration<double>(tick).count()));

    fake->injectRealtime(midi::MidiRealtime::Stop);
    CHECK_FALSE(store->clock().running);
    fake->injectRealtime(midi::MidiRealtime::Continue);
    CHECK(store->clock().running);
    CHECK(store->clock().ticks == 30);
    fake->injectRealtime(midi::MidiRealtime::Start);
    CHECK(store->clock().ticks == 0);
    CHECK(store->clock().starts == 2);
}
