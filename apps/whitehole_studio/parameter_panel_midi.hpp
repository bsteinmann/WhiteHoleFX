// parameter_panel_midi.hpp: der "Lernen"-Knopf für die MIDI-Nodes im Parameterfenster.
//
// Warum es das gibt: Kanal und Reglernummer von Hand einzutippen ist mühsam. Der Knopf stellt MidiLearn scharf,
// wartet auf die nächste passende Nachricht des Controllers und gibt sie an `apply` weiter, das Kanal und Nummer in den
// Node einträgt. Eigene Datei, damit parameter_panel.cpp nicht weiter wächst.
#pragma once

#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

#include "whitehole/midi/midi_learn.hpp"

namespace whitehole::studio {

/// Die Texte des Knopfes (übersetzt vom Aufrufer, damit sie in den Übersetzungsdateien landen).
struct MidiLearnTexts {
    QString idle;     // "Lernen"
    QString waiting;  // "Warte auf MIDI …"
    QString tooltip;
};

/// Liefert den Knopf. `isNote`: auf eine Taste statt auf einen Regler warten.
/// `apply` bekommt die gelernte Nachricht. Ein zweiter Klick bricht das Lernen ab.
QWidget* makeMidiLearnButton(std::shared_ptr<midi::MidiLearn> learn, bool isNote, MidiLearnTexts texts,
                             std::function<void(const midi::MidiMessage&)> apply, QWidget* parent);

}  // namespace whitehole::studio
