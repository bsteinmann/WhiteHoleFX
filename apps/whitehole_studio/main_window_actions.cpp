// main_window_actions.cpp: Menüs, Werkzeugleiste und Tastenkürzel des Hauptfensters.
// Menüs von links nach rechts: Datei, Bearbeiten, Stage Config, Ansicht, Wiedergabe, Live, Hilfe.
//
// Nur das Anlegen der Bedienelemente; was sie tun, steht in main_window*.cpp.
#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>

#include "main_window.hpp"
#include "node_editor.hpp"

namespace whitehole::studio {

void MainWindow::buildActions() {
    newAction_ = new QAction{tr("&Neu"), this};
    newAction_->setShortcut(QKeySequence::New);
    newAction_->setToolTip(tr("Neues Projekt (Strg+N)"));
    connect(newAction_, &QAction::triggered, this, &MainWindow::onNew);

    openAction_ = new QAction{tr("&Öffnen …"), this};
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::onOpen);

    reloadAction_ = new QAction{tr("&Neu laden"), this};
    reloadAction_->setShortcut(QKeySequence{Qt::Key_F5});
    connect(reloadAction_, &QAction::triggered, this, &MainWindow::onReload);

    saveAction_ = new QAction{tr("&Speichern"), this};
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, &MainWindow::onSave);

    saveAsAction_ = new QAction{tr("Speichern &unter …"), this};
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::onSaveAs);

    undoAction_ = new QAction{tr("&Rückgängig"), this};
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, &MainWindow::onUndo);
    redoAction_ = new QAction{tr("&Wiederholen"), this};
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, &MainWindow::onRedo);

    quitAction_ = new QAction{tr("&Beenden"), this};
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    runAction_ = new QAction{tr("&Start"), this};
    runAction_->setCheckable(true);
    runAction_->setShortcut(QKeySequence{Qt::Key_Space});
    connect(runAction_, &QAction::triggered, this, &MainWindow::onTogglePlayback);

    sendAction_ = new QAction{tr("&Ausgabe senden"), this};
    sendAction_->setCheckable(true);
    sendAction_->setToolTip(tr("Pakete (Art-Net, sACN, DDP) wirklich ins Netz schicken"));
    connect(sendAction_, &QAction::triggered, this, &MainWindow::onToggleSending);

    QMenu* file = menuBar()->addMenu(tr("&Datei"));
    // Datei: Anlegen und Öffnen, dann Speichern, dann Neu laden, zuletzt Beenden.
    file->addAction(newAction_);
    file->addAction(openAction_);
    recentMenu_ = file->addMenu(tr("&Zuletzt geöffnet"));
    connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::fillRecentMenu);
    fillExamplesMenu(file->addMenu(tr("&Beispiele")));
    file->addSeparator();
    file->addAction(saveAction_);
    file->addAction(saveAsAction_);
    file->addSeparator();
    file->addAction(reloadAction_);
    file->addSeparator();
    file->addAction(quitAction_);

    QMenu* edit = menuBar()->addMenu(tr("&Bearbeiten"));
    edit->addAction(undoAction_);
    edit->addAction(redoAction_);

    // Stage Config: einmal am Anfang einrichten (Fixtures, Patch, Testbilder), danach selten gebraucht.
    buildStageConfig(menuBar()->addMenu(tr("&Stage Config")));

    QMenu* view = menuBar()->addMenu(tr("&Ansicht"));
    view->addAction(editorDock_->toggleViewAction());
    view->addAction(dock_->toggleViewAction());
    view->addAction(lookDock_->toggleViewAction());

    // Reihenfolge wie im Betrieb: erst Wiedergabe und Ausgabe, dann der Live-Modus, ganz rechts die Hilfe.
    QMenu* run = menuBar()->addMenu(tr("&Wiedergabe"));
    run->addAction(runAction_);
    run->addAction(sendAction_);

    QMenu* live = menuBar()->addMenu(tr("&Live"));
    live->addAction(liveAction_);
    live->addAction(saveLookAction_);

    buildHelp(menuBar()->addMenu(tr("&Hilfe")));

    // Eine Leiste für Dateien (im Live-Modus ausgeblendet, damit auf der Bühne nichts versehentlich geöffnet wird) ...
    fileBar_ = addToolBar(tr("Datei"));
    fileBar_->setObjectName(QStringLiteral("fileBar"));
    fileBar_->setMovable(false);
    fileBar_->addAction(newAction_);
    fileBar_->addAction(openAction_);
    fileBar_->addAction(saveAction_);

    // ... und eine für den Betrieb: Start, Senden, Looks, Live; rechts Master und Blackout.
    QToolBar* bar = addToolBar(tr("Betrieb"));
    bar->setObjectName(QStringLiteral("runBar"));
    bar->setMovable(false);
    bar->addAction(runAction_);
    bar->addAction(sendAction_);
    // Wer sendet, soll es sehen: der Knopf leuchtet rot, solange die Pakete wirklich ins Netz gehen.
    sendButton_ = qobject_cast<QToolButton*>(bar->widgetForAction(sendAction_));
    if (sendButton_ != nullptr) {
        sendButton_->setStyleSheet(
            QStringLiteral("QToolButton:checked { background-color: #b71c1c; color: white; font-weight: bold; }"));
    }
    bar->addSeparator();
    bar->addAction(saveLookAction_);
    bar->addAction(liveAction_);

    buildControls(bar);  // Master und Blackout rechts in derselben Leiste
    buildView3d(view);   // Ansicht-Umschalter 2D/3D (Menü und Leiste)
    run->addSeparator();
    run->addAction(blackoutAction_);
    buildSettings(edit);  // Bearbeiten > Einstellungen; setzt die gemerkte Wahl um
    addShortcutHints();   // zuletzt: alle Aktionen existieren jetzt
}

void MainWindow::fillExamplesMenu(QMenu* menu) {
    // Installiert: <präfix>/share/whiteholefx/examples; Windows-Paket und Entwicklungsbaum:
    // "examples" neben dem Programm.
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList candidates{app + QStringLiteral("/../share/whiteholefx/examples"),
                                 app + QStringLiteral("/examples")};
    for (const QString& candidate : candidates) {
        const QDir dir{candidate};
        const QStringList files = dir.entryList({QStringLiteral("*.whfx")}, QDir::Files, QDir::Name);
        if (files.isEmpty()) {
            continue;
        }
        for (const QString& name : files) {
            const QString path = dir.absoluteFilePath(name);
            QAction* action = menu->addAction(QFileInfo{name}.completeBaseName());
            connect(action, &QAction::triggered, this, [this, path]() { openPath(path); });
        }
        return;
    }
    menu->setEnabled(false);
}

}  // namespace whitehole::studio
