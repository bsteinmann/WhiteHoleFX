// midi_learn.hpp: "Lernen" - die nächste passende MIDI-Nachricht als Zuweisung übernehmen.
//
// Warum es das gibt: Kanal und Reglernummer von Hand einzutippen ist mühsam und fehleranfällig. Stattdessen drückt man im
// Node "Lernen", bewegt am Controller den Regler (oder drückt die Taste), und die Werte stehen drin. Die Logik ist von der
// Oberfläche getrennt (und damit testbar): Sie vergleicht nur die laufende Nummer der letzten passenden Nachricht im
// MidiStore mit dem Stand beim Scharfstellen. Die Ereignisliste des Speichers bleibt unberührt, sie gehört dem Live-Modus.
#pragma once

#include <memory>
#include <optional>

#include "whitehole/midi/midi_store.hpp"

namespace whitehole::midi {

class MidiLearn {
public:
    enum class Want { Controller, Note };

    explicit MidiLearn(std::shared_ptr<MidiStore> store) : store_{std::move(store)} {}

    /// Stellt scharf: Nachrichten, die schon da sind, zählen nicht; nur eine neue.
    void arm(Want want);
    void cancel() noexcept { armed_ = false; }
    [[nodiscard]] bool armed() const noexcept { return armed_; }

    /// Liefert die erste neue passende Nachricht (Regler bzw. Note-An) und beendet das Lernen; sonst nichts.
    [[nodiscard]] std::optional<MidiMessage> poll();

private:
    [[nodiscard]] std::optional<LastMessage> latest() const;

    std::shared_ptr<MidiStore> store_;
    Want want_{Want::Controller};
    bool armed_{false};
    std::uint64_t baseline_{0};
};

}  // namespace whitehole::midi
