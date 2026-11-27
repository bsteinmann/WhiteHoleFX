// rtmidi_backend.cpp: MIDI-Ein- und Ausgang über RtMidi (ALSA unter Linux, WinMM unter Windows).
//
// Warum es das gibt: Das ist die einzige Stelle, die ein echtes Gerät berührt. RtMidi meldet Fehler als Ausnahmen;
// sie werden hier in Result umgewandelt, damit der Rest des Programms ausnahmefrei bleibt. SysEx und Active Sensing werden
// ignoriert; Takt-Meldungen (MIDI-Clock) gehen an den Echtzeit-Empfänger (Tempo-Uhr).
#include <exception>
#include <vector>

#include <RtMidi.h>

#include "whitehole/midi/midi_backend.hpp"

namespace whitehole::midi {

namespace {

std::vector<std::string> listPorts(RtMidi& device) {
    std::vector<std::string> names;
    try {
        const unsigned int count = device.getPortCount();
        names.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            names.push_back(device.getPortName(i));
        }
    } catch (const RtMidiError&) {
        names.clear();
    }
    return names;
}

class RtInput final : public IMidiInput {
public:
    RtInput() {
        try {
            device_ = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "WhiteHoleFX");
            device_->ignoreTypes(true, false, true);  // SysEx, Takt, Active Sensing: Takt durchlassen (MIDI-Clock)
        } catch (const RtMidiError&) {
            device_.reset();
        }
    }
    ~RtInput() override { close(); }

    std::vector<std::string> ports() override { return device_ ? listPorts(*device_) : std::vector<std::string>{}; }

    Result<void> open(const std::string& name, MidiHandler handler) override {
        if (!device_) {
            return Error{ErrorCode::Unsupported, "MIDI-Eingang nicht verfügbar"};
        }
        close();
        const int index = findMidiPort(ports(), name);
        if (index < 0) {
            return Error{ErrorCode::NotFound, "MIDI-Eingang nicht gefunden: " + name};
        }
        handler_ = std::move(handler);
        try {
            device_->setCallback(&RtInput::trampoline, this);
            device_->openPort(static_cast<unsigned int>(index), "WhiteHoleFX Eingang");
        } catch (const RtMidiError& error) {
            device_->cancelCallback();
            handler_ = nullptr;
            return Error{ErrorCode::IoError, error.getMessage()};
        }
        return {};
    }

    void close() override {
        if (!device_) {
            return;
        }
        try {
            if (device_->isPortOpen()) {
                device_->closePort();
            }
            device_->cancelCallback();
        } catch (const RtMidiError&) {
        }
        handler_ = nullptr;
    }

    bool isOpen() const override { return device_ && device_->isPortOpen(); }
    void setRealtimeHandler(MidiRealtimeHandler handler) override { realtime_ = std::move(handler); }

private:
    static void trampoline(double, std::vector<unsigned char>* bytes, void* self) {
        auto* input = static_cast<RtInput*>(self);
        if (bytes == nullptr || !input->handler_) {
            return;
        }
        if (const auto message = parseMidi(*bytes)) {
            input->handler_(*message);
        } else if (const auto realtime = parseMidiRealtime(*bytes); realtime && input->realtime_) {
            input->realtime_(*realtime);
        }
    }

    std::unique_ptr<RtMidiIn> device_;
    MidiHandler handler_;
    MidiRealtimeHandler realtime_;
};

class RtOutput final : public IMidiOutput {
public:
    RtOutput() {
        try {
            device_ = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, "WhiteHoleFX");
        } catch (const RtMidiError&) {
            device_.reset();
        }
    }
    ~RtOutput() override { close(); }

    std::vector<std::string> ports() override { return device_ ? listPorts(*device_) : std::vector<std::string>{}; }

    Result<void> open(const std::string& name) override {
        if (!device_) {
            return Error{ErrorCode::Unsupported, "MIDI-Ausgang nicht verfügbar"};
        }
        close();
        const int index = findMidiPort(ports(), name);
        if (index < 0) {
            return Error{ErrorCode::NotFound, "MIDI-Ausgang nicht gefunden: " + name};
        }
        try {
            device_->openPort(static_cast<unsigned int>(index), "WhiteHoleFX Ausgang");
        } catch (const RtMidiError& error) {
            return Error{ErrorCode::IoError, error.getMessage()};
        }
        return {};
    }

    void close() override {
        if (!device_) {
            return;
        }
        try {
            if (device_->isPortOpen()) {
                device_->closePort();
            }
        } catch (const RtMidiError&) {
        }
    }

    bool isOpen() const override { return device_ && device_->isPortOpen(); }

    Result<void> send(const MidiMessage& message) override {
        if (!isOpen()) {
            return Error{ErrorCode::InvalidArgument, "MIDI-Ausgang nicht geöffnet"};
        }
        const MidiBytes bytes = toBytes(message);
        const unsigned char data[3] = {bytes.status, bytes.data1, bytes.data2};
        try {
            device_->sendMessage(data, 3);
        } catch (const RtMidiError& error) {
            return Error{ErrorCode::IoError, error.getMessage()};
        }
        return {};
    }

private:
    std::unique_ptr<RtMidiOut> device_;
};

}  // namespace

bool midiDevicesAvailable() noexcept { return true; }
std::unique_ptr<IMidiInput> makeMidiInput() { return std::make_unique<RtInput>(); }
std::unique_ptr<IMidiOutput> makeMidiOutput() { return std::make_unique<RtOutput>(); }

}  // namespace whitehole::midi
