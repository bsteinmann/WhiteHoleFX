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
#include <QCheckBox>
#include <QDockWidget>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>

#include <algorithm>
#include <string>

#include "cue_panel.hpp"
#include "live_panel.hpp"
#include "look_panel.hpp"
#include <memory>
#include "main_window.hpp"
#include "stage_setup_window.hpp"

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
    // Die Schaltfläche "Einstellungen" im Live-Fenster blendet mit den Einstellungen auch die Menüleiste ein und aus
    connect(livePanel_, &LivePanel::settingsToggled, this, [this](bool shown) {
        if (liveMode_) {
            menuBar()->setVisible(shown);
        }
    });
    livePanel_->setLearning(library_->midi, library_->dmx,
                            [this](runtime::LiveFunction function, const LearnedSource& found) { learnLiveControl(function, found); });

    cuePanel_ = new CuePanel{*player_, this};
    connect(cuePanel_, &CuePanel::failed, this, toStatus);
    connect(cuePanel_, &CuePanel::changed, this, [this]() {
        updateTitle();
        updateActions();
    });
    connect(cuePanel_, &CuePanel::startRequested, this, [this]() { startCues(); });
    connect(cuePanel_, &CuePanel::stopRequested, this, [this]() { stopCues(); });
    cueDock_ = new QDockWidget{tr("Cuelist"), this};
    cueDock_->setWidget(cuePanel_);
    addDockWidget(Qt::RightDockWidgetArea, cueDock_);
    cueDock_->hide();  // erst auf Wunsch (Ansicht → Cuelist), damit das Fenster so bleibt, wie man es kennt

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
    addAction(liveAction_);  // F9 gilt auch bei ausgeblendeter Menüleiste
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
    setLiveChrome(true);
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
    setLiveChrome(false);
    showEditDocks();
    {
        const QSignalBlocker blocker{liveAction_};
        liveAction_->setChecked(false);
    }
    afterRestore();
    refreshImage();
}

void MainWindow::setLiveChrome(bool live) {
    // Erst die Einstellungen zu (liveMode_ entscheidet, ob das die Menüleiste anfasst), dann die Menüleiste selbst.
    livePanel_->setSettingsShown(false);
    menuBar()->setVisible(!live);
}

void MainWindow::hideEditDocks() {
    for (QAction* action : liveLockedActions_) {
        action->setEnabled(false);
    }
    runShortcut_ = runAction_->shortcut();
    runAction_->setShortcut(QKeySequence{});  // Leertaste ist im Live-Modus GO, nicht Start/Stopp
    hiddenByLive_.clear();
    stageWasOpen_ = stageSetup_->isVisible();  // auch das Fenster Stage Config: auf der Bühne wird nichts umgebaut
    stageSetup_->hide();
    stageMenu_->setEnabled(false);
    for (QDockWidget* dock : {editorDock_, dock_, lookDock_}) {
        if (dock->isVisible()) {
            hiddenByLive_.push_back(dock);
        }
        dock->hide();
    }
}

void MainWindow::showEditDocks() {
    for (QAction* action : liveLockedActions_) {
        action->setEnabled(true);
    }
    stageMenu_->setEnabled(true);
    if (!runShortcut_.isEmpty()) {
        runAction_->setShortcut(runShortcut_);
    }
    liveDock_->hide();
    for (QDockWidget* dock : hiddenByLive_) {
        dock->show();
    }
    hiddenByLive_.clear();
    if (stageWasOpen_) {
        stageSetup_->open(stageSetup_->page());
        stageWasOpen_ = false;
    }
}

void MainWindow::resetLive() {
    // Ein Projekt wurde geladen (der Player hat den Live-Betrieb schon beendet): Oberfläche nachziehen.
    if (liveMode_) {
        liveMode_ = false;
        setLiveChrome(false);
        showEditDocks();
        const QSignalBlocker blocker{liveAction_};
        liveAction_->setChecked(false);
    }
    lookPanel_->rebuild();
    livePanel_->rebuild();
    cuePanel_->rebuild();
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
    if (newAction_->isEnabled() || openAction_->isEnabled() || !runAction_->shortcut().isEmpty() || undoAction_->isEnabled()) {
        return tr("Live: Neu/Öffnen, Leertaste oder Rückgängig sind im Live-Modus nicht gesperrt");
    }
    // Auftrittsansicht: ohne Menüleiste und ohne Einstellungen; die Schaltfläche blendet beides gemeinsam ein
    QApplication::processEvents();  // Qt zeigt Widgets, die nach dem Anzeigen des Fensters entstehen, erst über die Ereignisschleife
    if (menuBar()->isVisibleTo(this) || livePanel_->settingsShown() || livePanel_->flashCheck(0) == nullptr ||
        livePanel_->flashCheck(0)->isVisibleTo(livePanel_)) {
        return QStringLiteral("Live: Menüleiste oder Einstellungen sind zu Beginn nicht ausgeblendet");
    }
    livePanel_->setSettingsShown(true);
    if (!menuBar()->isVisibleTo(this) || !livePanel_->flashCheck(0)->isVisibleTo(livePanel_)) {
        return QStringLiteral("Live: Die Einstellungen blenden Menüleiste und Flash-Haken nicht ein");
    }
    livePanel_->flashCheck(0)->click();
    if (player_->looks()[0].trigger != project::LookTrigger::Flash) {
        return QStringLiteral("Live: Der Flash-Haken stellt den Look nicht auf Flash");
    }
    livePanel_->flashCheck(0)->click();
    if (player_->looks()[0].trigger != project::LookTrigger::Toggle || livePanel_->flashCheck(0)->isChecked()) {
        return QStringLiteral("Live: Der Flash-Haken stellt den Look nicht auf Umschalten zurück");
    }
    livePanel_->setSettingsShown(false);
    if (menuBar()->isVisibleTo(this) || livePanel_->flashCheck(0)->isVisibleTo(livePanel_)) {
        return QStringLiteral("Live: Ausblenden der Einstellungen lässt Menüleiste oder Flash-Haken stehen");
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
    // Flash: Look 1 blitzt, solange gedrückt (Vorrang vor "Mit GO"), Loslassen kehrt zu Look 2 zurück (0 s: sofort)
    if (!livePanel_->setLookTrigger(0, project::LookTrigger::Flash, 0.0) || player_->looks()[0].trigger != project::LookTrigger::Flash) {
        return QStringLiteral("Live: Flash lässt sich nicht einstellen");
    }
    if (!livePanel_->setLookFlashIn(0, 0.0) || player_->looks()[0].flashInSeconds != 0.0) {
        return QStringLiteral("Live: Die Einblendzeit eines Flash-Looks lässt sich nicht einstellen");
    }
    if (const std::unique_ptr<QMenu> menu{livePanel_->lookMenu(0)}; menu->actions().size() < 6) {
        return QStringLiteral("Live: das Rechtsklick-Menü eines Looks ist unvollständig");
    }
    livePanel_->setUseGo(true);
    if (!livePanel_->press(0) || player_->liveStatus().current != 0) {
        return QStringLiteral("Live: Flash zeigt den Look beim Drücken nicht");
    }
    livePanel_->release(0);
    if (player_->liveStatus().current != 1) {
        return QStringLiteral("Live: Loslassen eines Flash-Looks kehrt nicht zurück");
    }
    livePanel_->setUseGo(false);
    (void)livePanel_->setLookTrigger(0, project::LookTrigger::Toggle, std::nullopt);
    if (player_->looks()[0].flashInSeconds.has_value()) {
        return QStringLiteral("Live: Umschalten behält die Einblendzeit eines Flash-Looks");
    }
    livePanel_->setSettingsShown(true);  // auch mit offenen Einstellungen muss der Editor die Menüleiste zurückbekommen
    leaveLive();
    if (player_->liveActive() || liveDock_->isVisible()) {
        return tr("Live: Zurück zum Editor scheitert");
    }
    if (!menuBar()->isVisibleTo(this) || livePanel_->settingsShown()) {
        return QStringLiteral("Live: Nach dem Live-Modus fehlt die Menüleiste oder die Einstellungen sind noch offen");
    }
    if (!newAction_->isEnabled() || !openAction_->isEnabled() || runAction_->shortcut().isEmpty() || !editorDock_->isVisible()) {
        return tr("Live: Neu/Öffnen, Leertaste oder Editor kommen nach dem Live-Modus nicht zurück");
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
