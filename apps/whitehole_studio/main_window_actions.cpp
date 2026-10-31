// main_window_actions.cpp: Menüs, Werkzeugleiste und Tastenkürzel des Hauptfensters.
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
    file->addAction(newAction_);
    file->addAction(openAction_);
    file->addAction(reloadAction_);
    file->addAction(saveAction_);
    file->addAction(saveAsAction_);
    fillExamplesMenu(file->addMenu(tr("&Beispiele")));
    file->addSeparator();
    file->addAction(quitAction_);

    QMenu* edit = menuBar()->addMenu(tr("&Bearbeiten"));
    edit->addAction(undoAction_);
    edit->addAction(redoAction_);

    QMenu* view = menuBar()->addMenu(tr("&Ansicht"));
    view->addAction(editorDock_->toggleViewAction());
    view->addAction(dock_->toggleViewAction());
    view->addAction(layoutDock_->toggleViewAction());
    view->addAction(testDock_->toggleViewAction());

    QMenu* run = menuBar()->addMenu(tr("&Wiedergabe"));
    run->addAction(runAction_);
    run->addAction(sendAction_);

    QToolBar* bar = addToolBar(tr("Wiedergabe"));
    bar->setMovable(false);
    bar->addAction(newAction_);
    bar->addAction(openAction_);
    bar->addAction(saveAction_);
    bar->addAction(runAction_);
    bar->addAction(sendAction_);
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
