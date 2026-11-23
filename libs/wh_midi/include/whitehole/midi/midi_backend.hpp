// midi_backend.hpp: die Schnittstelle zu MIDI-Geräten (Eingang und Ausgang) und die Suche nach einem Port.
//
// Warum es das gibt: Nur eine einzige Übersetzungseinheit (rtmidi_backend.cpp) berührt das Betriebssystem. Alles
// andere - Empfänger, Live-Modus, Tests - spricht gegen diese Schnittstelle; in den Tests steckt ein Fake dahinter,
// so läuft die Logik ohne Hardware. Ports werden über ihren Namen gewählt und gemerkt, nicht über eine Nummer, weil sich
// die Nummern ändern, sobald ein Gerät ab- und wieder angesteckt wird.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/midi/midi_message.hpp"

namespace whitehole::midi {

/// Der Rückruf läuft auf einem Thread des Backends, nicht auf dem Thread, der open() aufrief.
using MidiHandler = std::function<void(const MidiMessage&)>;

class IMidiInput {
public:
    virtual ~IMidiInput() = default;
    [[nodiscard]] virtual std::vector<std::string> ports() = 0;
    /// Öffnet den Port, dessen Name zu `name` passt (siehe findMidiPort). Fehler: NotFound, IoError.
    virtual Result<void> open(const std::string& name, MidiHandler handler) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
};

class IMidiOutput {
public:
    virtual ~IMidiOutput() = default;
    [[nodiscard]] virtual std::vector<std::string> ports() = 0;
    virtual Result<void> open(const std::string& name) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    /// Fehler: InvalidArgument (nicht geöffnet), IoError.
    virtual Result<void> send(const MidiMessage& message) = 0;
};

/// Index des Ports in `ports`, der zu `wanted` passt. Erst gleicher Name, dann gleicher Name ohne angehängte
/// Portnummer (Linux hängt " 20:0" an, und die Zahlen ändern sich beim nächsten Anstecken). -1, wenn nichts passt.
[[nodiscard]] int findMidiPort(const std::vector<std::string>& ports, std::string_view wanted);

/// Ob dieser Build echte Geräte ansprechen kann (false: nur das Dummy-Backend ohne Ports, z. B. ohne ALSA).
[[nodiscard]] bool midiDevicesAvailable() noexcept;
/// Backends über RtMidi. Ohne Geräteunterstützung liefern sie leere Port-Listen und Fehler beim Öffnen.
[[nodiscard]] std::unique_ptr<IMidiInput> makeMidiInput();
[[nodiscard]] std::unique_ptr<IMidiOutput> makeMidiOutput();

}  // namespace whitehole::midi
