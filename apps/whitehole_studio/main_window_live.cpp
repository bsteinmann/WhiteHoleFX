// main_window_live.cpp: Looks und Live-Betrieb im Hauptfenster.
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze. Hier steht alles, was Look-Liste
// (Dock "Looks", zum Bearbeiten) und Live-Panel (Dock "Live", zum Auftreten) mit dem Fenster verbindet:
// der Wechsel zwischen beiden Betriebsarten (F9), das Verstecken der Editor-Docks im Live-Betrieb und der
// Rauchtest dafür.
//
// Im Live-Betrieb ist das Fenster bewusst leer: Vorschau, große Look-Knöpfe, Master und Blackout. Was man
// im Editor ändern könnte, ist weg (Docks versteckt, Rückgängig gesperrt), damit auf der Bühne nichts
// versehentlich verstellt wird.
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QStatusBar>

#include <algorithm>
#include <string>

#include "live_panel.hpp"
#include "look_panel.hpp"
#include "main_window.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

void MainWindow::buildLive() {
    lookPanel_ = new LookPanel{*player_, this};
    livePanel_ = new LivePanel{*player_, this};
    const auto toStatus = [this](const QString& message) { statusBar()->showMessage(message, 8000); };
    connect(lookPanel_, &LookPanel::failed, this, toStatus);
    connect(livePanel_, &LivePanel::failed, this, toStatus);
    connect(lookPanel_, &LookPanel::changed, this, [this]() {
        livePanel_->rebuild();
        updateTitle();
        updateActions();
    });
    connect(lookPanel_, &LookPanel::loadedIntoEditor, this, &MainWindow::afterRestore);
    connect(lookPanel_, &LookPanel::liveRequested, this, [this](int first) { enterLive(first); });
    connect(livePanel_, &LivePanel::leaveRequested, this, [this]() { leaveLive(); });

    lookDock_ = new QDockWidget{tr("Looks"), this};
    lookDock_->setWidget(lookPanel_);
    addDockWidget(Qt::LeftDockWidgetArea, lookDock_);  // links, immer in Reichweite
    resizeDocks({lookDock_}, {250}, Qt::Horizontal);

    liveDock_ = new QDockWidget{tr("Live"), this};
    liveDock_->setWidget(livePanel_);
    liveDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
    liveDock_->setTitleBarWidget(new QWidget{liveDock_});  // keine Titelleiste: nichts zum Verschieben
    addDockWidget(Qt::BottomDockWidgetArea, liveDock_);
    liveDock_->hide();

    saveLookAction_ = new QAction{tr("&Look speichern …"), this};
    saveLookAction_->setShortcut(QKeySequence{Qt::CTRL | Qt::Key_L});
    saveLookAction_->setToolTip(tr("Den aktuellen Effekt als Look in die Liste legen (Strg+L)"));
    connect(saveLookAction_, &QAction::triggered, this, [this]() { lookPanel_->promptSave(); });

    liveAction_ = new QAction{tr("&Live-Modus"), this};
    liveAction_->setCheckable(true);
    liveAction_->setShortcut(QKeySequence{Qt::Key_F9});
    liveAction_->setToolTip(tr("Zwischen Bearbeiten und Live-Betrieb wechseln (F9)"));
    connect(liveAction_, &QAction::toggled, this, [this](bool on) {
        if (on) {
            enterLive(std::max(lookPanel_->selectedIndex(), 0));
        } else {
            leaveLive();
        }
    });
}

bool MainWindow::enterLive(int first) {
    if (liveMode_) {
        return true;
    }
    const auto setChecked = [this](bool on) {
        const QSignalBlocker blocker{liveAction_};
        liveAction_->setChecked(on);
    };
    if (!player_->status().loaded || player_->looks().empty()) {
        statusBar()->showMessage(tr("Für den Live-Modus erst Looks speichern (Dock „Looks“)."), 8000);
        setChecked(false);
        return false;
    }
    const Result<void> r = player_->startLive(first);
    if (!r.ok()) {
        statusBar()->showMessage(fromStd(r.error().message), 8000);
        setChecked(false);
        return false;
    }
    hideEditDocks();
    liveDock_->show();
    resizeDocks({liveDock_}, {320}, Qt::Vertical);
    liveMode_ = true;
    setChecked(true);
    livePanel_->rebuild();
    if (!player_->status().running) {
        startPlayback(false);
    }
    updateActions();
    return true;
}

void MainWindow::leaveLive() {
    if (!liveMode_) {
        return;
    }
    player_->stopLive();
    liveMode_ = false;
    showEditDocks();
    {
        const QSignalBlocker blocker{liveAction_};
        liveAction_->setChecked(false);
    }
    afterRestore();
    refreshImage();
}

void MainWindow::hideEditDocks() {
    fileBar_->hide();
    runShortcut_ = runAction_->shortcut();
    runAction_->setShortcut(QKeySequence{});  // Leertaste ist im Live-Modus GO, nicht Start/Stopp
    hiddenByLive_.clear();
    for (QDockWidget* dock : {editorDock_, dock_, layoutDock_, testDock_, lookDock_}) {
        if (dock->isVisible()) {
            hiddenByLive_.push_back(dock);
        }
        dock->hide();
    }
}

void MainWindow::showEditDocks() {
    fileBar_->show();
    if (!runShortcut_.isEmpty()) {
        runAction_->setShortcut(runShortcut_);
    }
    liveDock_->hide();
    for (QDockWidget* dock : hiddenByLive_) {
        dock->show();
    }
    hiddenByLive_.clear();
    dock_->raise();
}

void MainWindow::resetLive() {
    // Ein Projekt wurde geladen (der Player hat den Live-Betrieb schon beendet): Oberfläche nachziehen.
    if (liveMode_) {
        liveMode_ = false;
        showEditDocks();
        const QSignalBlocker blocker{liveAction_};
        liveAction_->setChecked(false);
    }
    lookPanel_->rebuild();
    livePanel_->rebuild();
}

QString MainWindow::exerciseLive() {
    if (!newProject(false)) {
        return tr("Live: Vorlage lässt sich nicht anlegen: %1").arg(problem());
    }
    if (!startPlayback(false)) {
        return tr("Live: Start scheitert: %1").arg(problem());
    }
    if (enterLive(0)) {
        return tr("Live: Modus ohne Looks hätte scheitern müssen");
    }
    if (!lookPanel_->saveLookAs(QStringLiteral("Eins")) || !lookPanel_->saveLookAs(QStringLiteral("Zwei"))) {
        return tr("Live: Looks lassen sich nicht speichern");
    }
    if (lookPanel_->lookCount() != 2 || livePanel_->buttonCount() != 2) {
        return tr("Live: Listen zeigen %1 / %2 statt 2 Looks").arg(lookPanel_->lookCount()).arg(livePanel_->buttonCount());
    }
    if (!lookPanel_->moveSelected(-1) || !lookPanel_->renameLook(0, QStringLiteral("Zwei neu"))) {
        return tr("Live: Ordnen oder Umbenennen scheitert");
    }
    if (!enterLive(0) || !player_->liveActive() || !liveDock_->isVisible() || editorDock_->isVisible()) {
        return tr("Live: Wechsel in den Live-Modus scheitert");
    }
    if (fileBar_->isVisible() || !runAction_->shortcut().isEmpty() || undoAction_->isEnabled()) {
        return tr("Live: Datei-Leiste, Leertaste oder Rückgängig sind im Live-Modus nicht gesperrt");
    }
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    // Direkt überblenden (Zeit des Looks), ein paar Frames mitlaufen lassen
    if (!livePanel_->trigger(1) || !player_->liveStatus().fading) {
        return tr("Live: Klick startet keine Überblendung");
    }
    for (int i = 0; i < 5; ++i) {
        tick();
        QApplication::processEvents();
    }
    // Mit GO: vorladen, dann auslösen; hart (0 s) ist sofort fertig
    livePanel_->setFadeOverride(0.0);
    livePanel_->setUseGo(true);
    if (!livePanel_->trigger(0) || player_->liveStatus().armed != 0) {
        return tr("Live: Vorladen merkt sich den Look nicht");
    }
    if (!livePanel_->go() || player_->liveStatus().current != 0 || player_->liveStatus().fading) {
        return tr("Live: GO mit 0 s schaltet nicht sofort um");
    }
    livePanel_->setUseGo(false);
    if (!livePanel_->trigger(1) || player_->liveStatus().current != 1) {
        return tr("Live: Direktes Umschalten mit 0 s scheitert");
    }
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    if (!problem().isEmpty()) {
        return tr("Live: %1").arg(problem());
    }
    leaveLive();
    if (player_->liveActive() || liveDock_->isVisible()) {
        return tr("Live: Zurück zum Editor scheitert");
    }
    if (!fileBar_->isVisible() || runAction_->shortcut().isEmpty() || !editorDock_->isVisible()) {
        return tr("Live: Datei-Leiste, Leertaste oder Editor kommen nach dem Live-Modus nicht zurück");
    }
    if (!lookPanel_->removeLook(0) || lookPanel_->lookCount() != 1) {
        return tr("Live: Look löschen scheitert");
    }
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    return problem();
}

}  // namespace whitehole::studio
