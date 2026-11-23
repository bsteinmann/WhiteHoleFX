// midi_live.hpp: der Live-Modus auf einem MIDI-Controller (Akai APC mini mk1) - Pads rufen Looks ab, die Pad-LEDs
// zeigen den Zustand, Tasten und Fader bedienen GO, Ende, Master, Überblendzeit und manuelle Überblendung.
//
// Warum es das gibt: Auf der Bühne will man Looks mit echten Tasten abrufen und sehen, was läuft, ohne auf den
// Bildschirm zu schauen. Die Belegung kommt aus einem Profil (midi/live_profile.hpp); die Logik hier ist vom Studio
// getrennt und damit ohne Gerät und ohne Qt testbar: Sie liest die Ereignisse aus dem MidiStore, ruft den Player auf
// (liveTrigger/liveArm/liveGo/liveManualFade/setMasterBrightness) und schickt nur geänderte LED-Werte an den Ausgang.
//
// Außerhalb des Live-Betriebs bleiben die Pads frei (Ereignisse werden verworfen, die MIDI-Nodes lesen den Zustand,
// nicht die Ereignisse). Nur die Stellung der Fader wird gemerkt, damit Überblendzeit und Mitnahme stimmen.
// "Live beenden" führt der Controller nicht selbst aus, sondern meldet es (das Studio räumt dabei seine Ansicht um).
#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "whitehole/midi/live_profile.hpp"
#include "whitehole/midi/midi_backend.hpp"
#include "whitehole/midi/midi_store.hpp"

namespace whitehole::runtime {

class Player;

struct MidiLiveOptions {
    bool enabled{true};                    // false: wie außerhalb des Live-Betriebs (Studio: nur im Live-Fenster aktiv)
    bool useGo{false};                     // Pad lädt nur vor (wie "Mit GO" im Live-Fenster)
    std::optional<double> fadeOverride;    // Überblendzeit des Live-Fensters; der Fader hat, einmal bewegt, Vorrang
};

struct MidiLiveResult {
    bool leaveRequested{false};  // Taste "Live beenden" gedrückt
    bool masterChanged{false};   // Master-Fader bewegt (die Anzeige im Studio nachziehen)
    std::optional<std::string> error;  // letzter Fehler eines Aufrufs (z. B. GO ohne vorgeladenen Look)
};

class MidiLiveController {
public:
    /// Fader-Wert, bis zu dem die manuelle Überblendung als "unten" gilt (Mitnahme, siehe docs/midi.md).
    static constexpr int kPickupThreshold = 3;
    static constexpr double kMaxFadeSeconds = 10.0;

    MidiLiveController(Player& player, std::shared_ptr<midi::MidiStore> store);

    /// Ausgang für die LEDs; nullptr = keine Rückmeldung. Erzwingt beim nächsten Schritt ein vollständiges Senden.
    void setOutput(midi::IMidiOutput* output);
    void setProfile(midi::LiveProfile profile);
    [[nodiscard]] const midi::LiveProfile& profile() const noexcept { return profile_; }

    /// Einmal pro Frame (nach Player::step oder davor - beides geht).
    MidiLiveResult step(const MidiLiveOptions& options);
    /// Löscht alle gesetzten LEDs (beim Beenden des Programms).
    void clear();

    /// Überblendzeit vom Fader, sobald er bewegt wurde.
    [[nodiscard]] std::optional<double> fadeFromFader() const noexcept { return fadeFader_; }

private:
    void handle(const midi::MidiMessage& message, const MidiLiveOptions& options, bool live, MidiLiveResult& result);
    void handleCrossfade(int value, MidiLiveResult& result);
    void syncArmed();
    void updateLeds(bool live);
    void report(const Result<void>& result, MidiLiveResult& out);

    Player& player_;
    std::shared_ptr<midi::MidiStore> store_;
    midi::IMidiOutput* output_{nullptr};
    midi::LiveProfile profile_;
    std::array<int, 128> sent_{};  // zuletzt gesendete Velocity je Note; -1 = unbekannt
    bool wasLive_{false};
    std::optional<double> fadeFader_;
    int crossfade_{-1};  // letzte Stellung des Überblend-Faders; -1 = unbekannt
    bool crossfadeReady_{false};
    int armedLook_{-1};  // vorgeladen bzw. beim manuellen Überblenden im Kommen
};

}  // namespace whitehole::runtime
