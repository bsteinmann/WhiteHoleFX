// midi_receiver.hpp: verbindet einen MIDI-Eingang mit dem MidiStore.
//
// Warum es das gibt: Wie der DmxReceiver trennt er "Gerät öffnen und zuhören" von "Zustand merken". Er kennt nur die
// Schnittstelle IMidiInput, ist also mit einem Fake ohne Hardware testbar. Die Oberfläche startet und stoppt ihn
// beim Ändern der Einstellungen.
#pragma once

#include <memory>
#include <string>

#include "whitehole/midi/midi_backend.hpp"
#include "whitehole/midi/midi_store.hpp"

namespace whitehole::midi {

class MidiReceiver {
public:
    MidiReceiver(std::unique_ptr<IMidiInput> input, std::shared_ptr<MidiStore> store);
    ~MidiReceiver();
    MidiReceiver(const MidiReceiver&) = delete;
    MidiReceiver& operator=(const MidiReceiver&) = delete;

    /// Öffnet den Port (schließt vorher einen offenen). Leerer Name schaltet nur ab. Fehler wie IMidiInput::open.
    Result<void> start(const std::string& portName);
    void stop();
    [[nodiscard]] bool running() const { return input_->isOpen(); }
    [[nodiscard]] std::vector<std::string> ports() { return input_->ports(); }
    [[nodiscard]] const std::shared_ptr<MidiStore>& store() const { return store_; }

private:
    std::unique_ptr<IMidiInput> input_;
    std::shared_ptr<MidiStore> store_;
};

}  // namespace whitehole::midi
