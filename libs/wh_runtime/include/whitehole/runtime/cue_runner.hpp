// cue_runner.hpp: der Ablauf einer Cuelist als reine Zustandsmaschine (Issue #21).
//
// Warum es diese Datei gibt: Welcher Cue wann dran ist, hängt von Auslösern ab (Knopf, Zeit, Taste, DMX-Wert, Beat) und
// von Zeiten (Delay, Fade). Das ist Logik ohne GPU, ohne Uhr und ohne Netzwerk: Die Zeit und die Ereignisse kommen als
// Parameter herein, heraus kommt "führe jetzt Cue N aus". Dadurch lässt sich jeder Fall genau testen; der Player
// (player_cues.cpp) übersetzt nur noch in Look-Wechsel auf den Live-Decks.
//
// Ablauf wie am Pult: Es gibt immer einen "nächsten" Cue (nach dem zuletzt ausgelösten). Er wartet auf seinen Auslöser:
//   Go     nur auf den GO-Knopf,           Time   auf `time` Sekunden nach dem Ende der vorigen Überblendung,
//   Key    auf seine Taste (von jedem Stand aus, springt auch rückwärts),
//   Dmx    auf einen Anstieg des Kanals auf die Schwelle (ein schon hoher Regler löst nicht aus),
//   Audio  auf den nächsten Beat.
// Der GO-Knopf löst den nächsten Cue immer aus, gleich welcher Auslöser dort steht. Nach dem Auslösen vergeht `delay`,
// dann soll die Überblendung beginnen (execute); ein neuer Auslöser in der Wartezeit ersetzt den wartenden Cue.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "whitehole/project/cue.hpp"

namespace whitehole::runtime {

/// Was seit dem letzten update() von außen geschah.
struct CueEvents {
    bool go{false};                  // GO-Knopf
    std::vector<std::string> keys;   // gedrückte Tasten (Namen wie im Cue)
    bool beat{false};                // seit dem letzten Aufruf kam mindestens ein Beat
    /// Wert (0..255) des DMX-Kanals, den der Cue beobachtet; leer, wenn kein frisches Signal da ist.
    std::function<std::optional<int>(const project::Cue&)> dmx;
};

struct CueRunnerStatus {
    bool running{false};
    int current{-1};   // zuletzt ausgelöster Cue (auch wenn seine Überblendung noch wartet); -1 = keiner
    int next{-1};      // wartet auf seinen Auslöser; -1 = Ende der Liste
    int pending{-1};   // ausgelöst, Delay läuft noch
    double pendingSeconds{0.0};  // Rest des Delays
    bool fading{false};          // die Überblendung des aktuellen Cues läuft
    double fadeProgress{1.0};    // 0..1
};

class CueRunner {
public:
    /// Setzt die Liste und hält an.
    void load(std::vector<project::Cue> cues);
    [[nodiscard]] const std::vector<project::Cue>& cues() const noexcept { return cues_; }

    /// Beginnt bei Cue 0, der sofort und ohne Überblendung gilt (der Player zeigt seinen Look direkt).
    /// Ohne Cues ein No-op. Liefert, ob die Liste läuft.
    bool start(double now);
    void stop();
    [[nodiscard]] bool running() const noexcept { return running_; }

    /// Löst einen bestimmten Cue aus (Doppelklick in der Liste): Delay, dann Überblendung.
    /// Außerhalb 0..n-1 oder angehalten: false.
    bool fire(int index, double now);

    /// Zeit und Ereignisse einarbeiten. Liefert den Cue, dessen Überblendung jetzt beginnen soll.
    [[nodiscard]] std::optional<int> update(double now, const CueEvents& events);

    [[nodiscard]] CueRunnerStatus status(double now) const;

private:
    [[nodiscard]] int nextIndex() const noexcept;
    void trigger(int index, double now);

    std::vector<project::Cue> cues_;
    bool running_{false};
    int current_{-1};
    int pending_{-1};
    double pendingDue_{0.0};
    double fadeStart_{0.0};
    double fadeEnd_{0.0};  // Ende der Überblendung des aktuellen Cues (Bezug für Time-Auslöser)
    // DMX-Flanke: für welchen Cue der letzte Zustand gilt und ob er hoch war
    int dmxFor_{-1};
    bool dmxHigh_{false};
};

}  // namespace whitehole::runtime
