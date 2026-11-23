// midi_backend_stub.cpp: MIDI ohne Geräteunterstützung (kein ALSA/WinMM im Build, oder -DWHITEHOLEFX_MIDI_DEVICES=OFF).
// Warum es das gibt: Das Programm soll sich überall bauen lassen, auch wo RtMidi nicht gebaut werden kann; MIDI-Ports
// sind dann einfach nicht vorhanden, und alles andere (Store, Nodes, Tests mit Fake) funktioniert unverändert.
#include "whitehole/midi/midi_backend.hpp"

namespace whitehole::midi {

namespace {

class StubInput final : public IMidiInput {
public:
    std::vector<std::string> ports() override { return {}; }
    Result<void> open(const std::string&, MidiHandler) override {
        return Error{ErrorCode::Unsupported, "Dieser Build hat keine MIDI-Geräteunterstützung"};
    }
    void close() override {}
    bool isOpen() const override { return false; }
};

class StubOutput final : public IMidiOutput {
public:
    std::vector<std::string> ports() override { return {}; }
    Result<void> open(const std::string&) override {
        return Error{ErrorCode::Unsupported, "Dieser Build hat keine MIDI-Geräteunterstützung"};
    }
    void close() override {}
    bool isOpen() const override { return false; }
    Result<void> send(const MidiMessage&) override {
        return Error{ErrorCode::Unsupported, "Dieser Build hat keine MIDI-Geräteunterstützung"};
    }
};

}  // namespace

bool midiDevicesAvailable() noexcept { return false; }
std::unique_ptr<IMidiInput> makeMidiInput() { return std::make_unique<StubInput>(); }
std::unique_ptr<IMidiOutput> makeMidiOutput() { return std::make_unique<StubOutput>(); }

}  // namespace whitehole::midi
