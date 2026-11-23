// live_learn.hpp: "Lernen" für die Live-Steuerung (GO, Live beenden, Master, Überblendzeit, manuelle Überblendung) und
// die Texte, mit denen Reiter "Steuerung" und Live-Fenster eine Belegung anzeigen.
//
// Warum es das gibt: Reiter und Live-Fenster schreiben dieselbe Belegung (runtime::LiveBindings) und sollen sich gleich
// bedienen. Der Knopf hier stellt MIDI- und/oder DMX-Lernen scharf und meldet die erste passende Bewegung: Tasten lernen
// eine Note, Fader einen Regler (CC); bei DMX den Kanal, der sich am stärksten bewegt (input::DmxLearn).
#pragma once

#include <QPushButton>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

#include "whitehole/input/dmx_learn.hpp"
#include "whitehole/midi/midi_learn.hpp"
#include "whitehole/runtime/live_bindings.hpp"

namespace whitehole::studio {

struct LearnedSource {
    std::optional<runtime::MidiBinding> midi;
    std::optional<input::DmxAddress> dmx;
};

/// Name der Funktion für die Oberfläche ("GO", "Live beenden", ...).
[[nodiscard]] QString liveFunctionName(runtime::LiveFunction function);
/// "Note 82, Kanal 1" bzw. "Regler 48, Kanal 1"; "aus" bei Nummer -1.
[[nodiscard]] QString describeMidi(const runtime::MidiBinding& binding, runtime::LiveFunction function);
/// "Art-Net 0 / 5" bzw. "sACN 3 / 120".
[[nodiscard]] QString describeDmx(const input::DmxAddress& address);

/// Ein Knopf "Lernen" (umschaltbar; zweiter Klick bricht ab), mit eigenem Lernen je Knopf. `midi`/`dmx` dürfen fehlen,
/// dann lernt er nur das andere; fehlen beide, ist er gesperrt. `done` bekommt die erste gefundene Quelle.
QPushButton* makeLiveLearnButton(runtime::LiveFunction function, std::shared_ptr<midi::MidiStore> midi,
                                 std::shared_ptr<const input::DmxStore> dmx, std::function<void(const LearnedSource&)> done,
                                 QWidget* parent);

/// Ob gerade irgendein Lernen-Knopf wartet. Solange, wirkt der Controller im Live-Modus nicht (sonst löste die Taste,
/// die man gerade lernt, auch ihre alte Funktion aus).
[[nodiscard]] bool liveLearnActive() noexcept;

}  // namespace whitehole::studio
