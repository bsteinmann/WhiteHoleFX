// main_window_midi.cpp: MIDI-Ein- und -Ausgang des Hauptfensters (Teil von MainWindow).
//
// Warum es das gibt: Welcher Controller benutzt wird, gehört zu den Programmeinstellungen (siehe AppSettings). Das Öffnen
// der Ports und der Umgang mit Fehlern steht hier, damit main_window_settings.cpp übersichtlich bleibt. Der Empfänger
// (wh_midi) schreibt in den MidiStore; die MIDI-Nodes und der Live-Modus lesen später daraus.
#include <QStringList>

#include <string>

#include "main_window.hpp"
#include "whitehole/midi/midi_backend.hpp"

namespace whitehole::studio {

QString MainWindow::applyMidi() {
    if (!midiStore_) {
        midiStore_ = std::make_shared<midi::MidiStore>();
    }
    if (!midiReceiver_) {
        midiReceiver_ = std::make_unique<midi::MidiReceiver>(midi::makeMidiInput(), midiStore_);
    }
    if (!midiOutput_) {
        midiOutput_ = midi::makeMidiOutput();
    }
    QStringList problems;
    const Result<void> in = midiReceiver_->start(appSettings_.midiInput.toStdString());
    if (!in.ok()) {
        problems << tr("MIDI-Eingang: %1").arg(QString::fromStdString(in.error().message));
    }
    midiOutput_->close();
    if (!appSettings_.midiOutput.isEmpty()) {
        const Result<void> out = midiOutput_->open(appSettings_.midiOutput.toStdString());
        if (!out.ok()) {
            problems << tr("MIDI-Ausgang: %1").arg(QString::fromStdString(out.error().message));
        }
    }
    return problems.join(QLatin1Char('\n'));
}

bool MainWindow::midiNeedsApply(const AppSettings& before) const {
    if (appSettings_.midiInput != before.midiInput || appSettings_.midiOutput != before.midiOutput) {
        return true;
    }
    // Gemerkter Port, der noch nicht läuft (Gerät war beim Start nicht da): bei jedem Übernehmen noch einmal versuchen.
    return (!appSettings_.midiInput.isEmpty() && (!midiReceiver_ || !midiReceiver_->running())) ||
           (!appSettings_.midiOutput.isEmpty() && (!midiOutput_ || !midiOutput_->isOpen()));
}

}  // namespace whitehole::studio
