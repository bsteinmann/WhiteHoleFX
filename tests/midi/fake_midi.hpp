// fake_midi.hpp: ein MIDI-Gerät ohne Hardware für die Tests.
// Warum es das gibt: Empfänger, Live-Modus und Einstellungen sollen ohne echten Controller prüfbar sein. Der Fake
// listet feste Ports, lässt Nachrichten "von außen" einspeisen und merkt sich alles, was gesendet wurde.
#pragma once

#include <string>
#include <vector>

#include "whitehole/midi/midi_backend.hpp"

namespace whitehole::midi::testing {

class FakeMidiInput final : public IMidiInput {
public:
    std::vector<std::string> portNames{"Fake Eingang 20:0"};

    std::vector<std::string> ports() override { return portNames; }
    Result<void> open(const std::string& name, MidiHandler handler) override {
        close();
        if (findMidiPort(portNames, name) < 0) {
            return Error{ErrorCode::NotFound, "kein Port"};
        }
        handler_ = std::move(handler);
        open_ = true;
        openedName = name;
        return {};
    }
    void close() override {
        open_ = false;
        handler_ = nullptr;
    }
    bool isOpen() const override { return open_; }
    void setRealtimeHandler(MidiRealtimeHandler handler) override { realtime_ = std::move(handler); }

    /// Spielt eine Echtzeitmeldung (MIDI-Clock) ein, als käme sie vom Gerät.
    void injectRealtime(MidiRealtime event) const {
        if (open_ && realtime_) {
            realtime_(event);
        }
    }

    /// Spielt eine Nachricht ein, als käme sie vom Gerät. Ohne offenen Port passiert nichts.
    void inject(const MidiMessage& message) const {
        if (open_ && handler_) {
            handler_(message);
        }
    }

    std::string openedName;

private:
    MidiHandler handler_;
    MidiRealtimeHandler realtime_;
    bool open_{false};
};

class FakeMidiOutput final : public IMidiOutput {
public:
    std::vector<std::string> portNames{"Fake Ausgang 20:1"};
    std::vector<MidiMessage> sent;

    std::vector<std::string> ports() override { return portNames; }
    Result<void> open(const std::string& name) override {
        if (findMidiPort(portNames, name) < 0) {
            return Error{ErrorCode::NotFound, "kein Port"};
        }
        open_ = true;
        return {};
    }
    void close() override { open_ = false; }
    bool isOpen() const override { return open_; }
    Result<void> send(const MidiMessage& message) override {
        if (!open_) {
            return Error{ErrorCode::InvalidArgument, "nicht geöffnet"};
        }
        sent.push_back(message);
        return {};
    }

private:
    bool open_{false};
};

}  // namespace whitehole::midi::testing
