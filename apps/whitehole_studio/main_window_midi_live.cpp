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
#include <string>
#include <vector>

#include "live_panel.hpp"
#include "main_window.hpp"
#include "live_controls_page.hpp"
#include "live_learn.hpp"
#include "midi_profile_box.hpp"
#include "settings_dialog.hpp"

namespace whitehole::studio {

void MainWindow::applyMidiProfile() {
    if (!midiLive_) {
        midiLive_ = std::make_unique<runtime::MidiLiveController>(*player_, library_->midi);
        if (midiOutput_ && midiOutput_->isOpen()) {
            midiLive_->setOutput(midiOutput_.get());
        }
    }
    midiLive_->setProfile(findLiveProfile(appSettings_.midiProfiles, appSettings_.midiProfile));
    midiLive_->setDmxStore(library_->dmx);
    std::vector<std::string> lines;
    for (const QString& line : appSettings_.liveBindings) {
        lines.push_back(line.toStdString());
    }
    if (const runtime::LiveBindings bindings = runtime::parseBindings(lines); !(bindings == midiLive_->bindings())) {
        midiLive_->setBindings(bindings);
    }
}

void MainWindow::learnLiveControl(runtime::LiveFunction function, const LearnedSource& found) {
    AppSettings next = appSettings_;
    std::vector<std::string> lines;
    for (const QString& line : next.liveBindings) {
        lines.push_back(line.toStdString());
    }
    runtime::LiveBindings bindings = runtime::parseBindings(lines);
    QString what;
    if (found.midi) {
        bindings.midiOf(function) = *found.midi;
        what = describeMidi(*found.midi, function);
    } else if (found.dmx && runtime::hasDmxBinding(function)) {
        bindings.dmxOf(function) = *found.dmx;
        what = describeDmx(*found.dmx);
    } else if (found.dmx) {  // Master über DMX: die vorhandene Einstellung der Seite DMX-Eingang
        next.masterDmx = true;
        next.masterDmxProtocol = found.dmx->protocol == input::DmxProtocol::Sacn ? 1 : 0;
        next.masterDmxUniverse = static_cast<int>(found.dmx->universe);
        next.masterDmxChannel = static_cast<int>(found.dmx->channel);
        what = describeDmx(*found.dmx);
    } else {
        return;
    }
    next.liveBindings.clear();
    for (const std::string& line : runtime::serializeBindings(bindings)) {
        next.liveBindings << QString::fromStdString(line);
    }
    const QString problems = applySettings(next, true);
    statusBar()->showMessage(problems.isEmpty() ? tr("%1: %2 gelernt").arg(liveFunctionName(function), what) : problems, 6000);
}

void MainWindow::midiTick() {
    if (!midiLive_) {
        applyMidiProfile();
    }
    runtime::MidiLiveOptions options;
    options.enabled = liveMode_ && !liveLearnActive();  // beim Lernen löst die Taste nicht ihre alte Funktion aus
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
    // 3. Reiter "Steuerung": lernen, zurücksetzen, Master über DMX landet auf der Seite DMX-Eingang.
    {
        SettingsDialog dialog{appSettings_, *library_->registry, this};
        LiveControlsPage* page = dialog.liveControlsPage();
        page->learnMidi(runtime::LiveFunction::Go, runtime::MidiBinding{2, 40});
        page->learnDmx(runtime::LiveFunction::Crossfade, input::DmxAddress{input::DmxProtocol::ArtNet, 3, 9});
        page->learnDmx(runtime::LiveFunction::Master, input::DmxAddress{input::DmxProtocol::Sacn, 4, 7});
        const AppSettings learned = dialog.settings();
        if (!learned.liveBindings.contains(QStringLiteral("go midi 2 40")) ||
            !learned.liveBindings.contains(QStringLiteral("crossfade dmx artnet 3 9")) || !learned.masterDmx ||
            learned.masterDmxProtocol != 1 || learned.masterDmxUniverse != 4 || learned.masterDmxChannel != 7) {
            return QStringLiteral("Steuerung: gelernte Belegung kommt nicht in den Einstellungen an");
        }
        page->resetMidi(runtime::LiveFunction::Go);
        page->clearDmx(runtime::LiveFunction::Master);
        if (dialog.settings().liveBindings.contains(QStringLiteral("go midi 2 40")) || dialog.settings().masterDmx ||
            !page->midiText(runtime::LiveFunction::Go).contains(QStringLiteral("82"))) {
            return QStringLiteral("Steuerung: Zurücksetzen auf Profil bzw. Aus wirkt nicht");
        }
    }
    // 4. Lernen im Live-Fenster schreibt dieselbe Belegung und erreicht den Controller.
    learnLiveControl(runtime::LiveFunction::Stop, LearnedSource{runtime::MidiBinding{1, 20}, std::nullopt});
    if (!appSettings_.liveBindings.contains(QStringLiteral("stop midi 1 20")) ||
        !(midiLive_->bindings().midiOf(runtime::LiveFunction::Stop) == runtime::MidiBinding{1, 20})) {
        return QStringLiteral("Steuerung: Lernen im Live-Fenster kommt nicht an");
    }
    (void)applySettings(before, true);
    // 5. Außerhalb des Live-Modus bleibt ein Pad wirkungslos.
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
