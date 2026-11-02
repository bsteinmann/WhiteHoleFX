// main_window_cues.cpp: Cuelist im Hauptfenster (Issue #21): Start, Stopp und der Rauchtest dafür.
//
// Warum eine eigene Datei: main_window_live.cpp ist mit den Looks gut gefüllt. Die Cuelist läuft im Live-Modus des Players
// (zwei Decks mit Überblendung); Start wechselt dorthin, Stopp lässt den Live-Modus stehen, damit man danach
// mit den Look-Knöpfen weitermachen kann. Zurück zum Editor geht wie immer mit F9 oder Esc.
#include <QApplication>
#include <QDockWidget>
#include <QStatusBar>

#include <string>

#include "cue_panel.hpp"
#include "live_panel.hpp"
#include "look_panel.hpp"
#include "main_window.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

void MainWindow::startCues() {
    if (player_->cues().empty()) {
        statusBar()->showMessage(tr("Die Cuelist ist leer. Erst Cues anlegen (Dock „Cuelist“)."), 8000);
        cuePanel_->rebuild();
        return;
    }
    if (!liveMode_) {
        // Der Live-Modus beginnt mit dem Look des ersten Cues (ohne Look: dem ersten der Liste).
        int first = 0;
        const project::Cue& firstCue = player_->cues().front();
        for (std::size_t i = 0; i < player_->looks().size(); ++i) {
            if (player_->looks()[i].name == firstCue.look) {
                first = static_cast<int>(i);
            }
        }
        if (!enterLive(first)) {
            cuePanel_->rebuild();
            return;
        }
    }
    if (const Result<void> started = player_->cuesStart(); !started.ok()) {
        statusBar()->showMessage(fromStd(started.error().message), 8000);
    }
    cueDock_->show();
    cuePanel_->rebuild();
}

void MainWindow::stopCues() {
    player_->cuesStop();
    cuePanel_->rebuild();
}

QString MainWindow::exerciseCues() {
    if (!newProject(false)) {
        return tr("Cuelist: Vorlage lässt sich nicht anlegen: %1").arg(problem());
    }
    if (!startPlayback(false)) {
        return tr("Cuelist: Start scheitert: %1").arg(problem());
    }
    if (!lookPanel_->saveLookAs(QStringLiteral("CueA")) || !lookPanel_->saveLookAs(QStringLiteral("CueB"))) {
        return tr("Cuelist: Looks lassen sich nicht speichern");
    }
    // Anlegen, ändern, löschen, ordnen
    for (int i = 0; i < 4; ++i) {
        if (!cuePanel_->addCue()) {
            return tr("Cuelist: Cue lässt sich nicht anlegen");
        }
    }
    project::Cue withB = player_->cues()[1];
    withB.name = "Zwei";
    withB.look = "CueB";
    withB.fade = 0.0;
    project::Cue black = player_->cues()[2];
    black.look.clear();  // Schwarz
    black.fade = 0.0;
    if (!cuePanel_->applyCue(1, withB) || !cuePanel_->applyCue(2, black) || !cuePanel_->applyCue(3, black)) {
        return tr("Cuelist: Cue lässt sich nicht ändern");
    }
    if (!cuePanel_->removeSelected() || cuePanel_->cueCount() != 3) {  // gewählt ist die Zeile 4
        return tr("Cuelist: Cue lässt sich nicht löschen");
    }
    if (!cuePanel_->moveSelected(-1) || !player_->cues()[1].look.empty() || player_->cues()[2].look != "CueB") {
        return tr("Cuelist: Cue lässt sich nicht verschieben");
    }
    // Ablauf: Start zeigt Cue 1, GO schaltet weiter (Schwarz, dann CueB), Looks von Hand sind gesperrt
    startCues();
    if (!player_->cuesRunning() || !liveMode_ || !cueDock_->isVisible()) {
        return tr("Cuelist: Start scheitert: %1").arg(problem());
    }
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    if (player_->cueStatus().current != 0 || player_->liveStatus().current != 0) {
        return tr("Cuelist: Cue 1 zeigt nicht den ersten Look");
    }
    if (livePanel_->trigger(1)) {
        return tr("Cuelist: Look-Knöpfe hätten gesperrt sein müssen");
    }
    player_->cueGo();
    tick();
    if (player_->cueStatus().current != 1) {
        return tr("Cuelist: GO schaltet nicht zum zweiten Cue");
    }
    player_->cueGo();
    tick();
    QApplication::processEvents();
    if (player_->cueStatus().current != 2 || player_->liveStatus().current != 1 || player_->cueError()) {
        return tr("Cuelist: GO schaltet nicht zum dritten Cue");
    }
    stopCues();
    if (player_->cuesRunning() || !livePanel_->trigger(0)) {
        return tr("Cuelist: Stopp gibt die Look-Knöpfe nicht frei");
    }
    leaveLive();
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    return problem();
}

}  // namespace whitehole::studio
