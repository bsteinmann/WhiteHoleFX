// main_window_midi_live.cpp: der Live-Modus auf dem MIDI-Controller im Hauptfenster (Teil von MainWindow).
//
// Warum es das gibt: Die Logik (Pads -> Looks, LEDs, Fader) steckt testbar in runtime::MidiLiveController. Hier wird
// sie mit dem Studio verbunden: Profil aus den Einstellungen, "Mit GO" und eigene Überblendzeit aus dem Live-Fenster,
// Master-Regler nachziehen, "Live beenden" wie der Knopf im Live-Fenster. Aktiv nur, solange das Live-Fenster offen ist.
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>

#include <cmath>

#include "live_panel.hpp"
#include "main_window.hpp"
#include "midi_profile_box.hpp"

namespace whitehole::studio {

void MainWindow::applyMidiProfile() {
    if (!midiLive_) {
        midiLive_ = std::make_unique<runtime::MidiLiveController>(*player_, library_->midi);
        if (midiOutput_ && midiOutput_->isOpen()) {
            midiLive_->setOutput(midiOutput_.get());
        }
    }
    midiLive_->setProfile(findLiveProfile(appSettings_.midiProfiles, appSettings_.midiProfile));
}

void MainWindow::midiTick() {
    if (!midiLive_) {
        applyMidiProfile();
    }
    runtime::MidiLiveOptions options;
    options.enabled = liveMode_;
    if (liveMode_ && livePanel_ != nullptr) {
        options.useGo = livePanel_->useGo();
        options.fadeOverride = livePanel_->fadeOverride();
    }
    const runtime::MidiLiveResult result = midiLive_->step(options);
    if (result.error) {
        statusBar()->showMessage(tr("MIDI: %1").arg(QString::fromStdString(*result.error)), 5000);
    }
    if (result.masterChanged && masterSlider_ != nullptr && masterLabel_ != nullptr) {
        const int percent = static_cast<int>(std::lround(player_->masterBrightness() * 100.0F));
        const QSignalBlocker block{masterSlider_};
        masterSlider_->setValue(percent);
        masterLabel_->setText(tr("%1 %").arg(percent));
    }
    if (result.leaveRequested && liveMode_) {
        leaveLive();
    }
}

QString MainWindow::exerciseMidiLive() {
    // 1. Profilverwaltung ohne Rückfragen: kopieren, doppelte Namen ablehnen, umbenennen, vorgefertigte schützen, löschen.
    MidiProfileBox box{QString{}, QStringList{}, this};
    const QString builtin = box.active();
    if (!box.copyCurrent(QStringLiteral("Eigen")) || box.copyCurrent(QStringLiteral("Eigen")) ||
        !box.renameCurrent(QStringLiteral("Eigen 2")) || box.userProfiles().size() != 1) {
        return QStringLiteral("MIDI-Profile: Kopieren/Umbenennen scheitert");
    }
    if (!box.select(builtin) || box.removeCurrent() || box.renameCurrent(QStringLiteral("x"))) {
        return QStringLiteral("MIDI-Profile: ein vorgefertigtes Profil ließ sich ändern");
    }
    if (!box.select(QStringLiteral("Eigen 2")) || !box.removeCurrent() || !box.userProfiles().isEmpty()) {
        return QStringLiteral("MIDI-Profile: Löschen scheitert");
    }
    // 2. Das gewählte Profil erreicht den Controller.
    const AppSettings before = appSettings_;
    AppSettings wide = before;
    wide.midiProfile = QStringLiteral("Looks 8 x 8 ohne Steuertasten");
    (void)applySettings(wide, false);
    if (!midiLive_ || midiLive_->profile().columns != 8) {
        return QStringLiteral("MIDI-Profile: der Controller kennt das gewählte Profil nicht");
    }
    (void)applySettings(before, false);
    // 3. Außerhalb des Live-Modus bleibt ein Pad wirkungslos.
    if (!liveMode_) {
        library_->midi->apply(midi::noteOn(0, 56, 127));
        midiTick();
        if (player_->liveActive()) {
            return QStringLiteral("MIDI-Profile: ein Pad hat außerhalb des Live-Modus etwas ausgelöst");
        }
    }
    return {};
}

}  // namespace whitehole::studio
