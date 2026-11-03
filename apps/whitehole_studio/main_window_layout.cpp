// main_window_layout.cpp: merkt sich die Fensteranordnung zwischen zwei Sitzungen (Issue #27).
//
// Warum eigene Datei: main_window.cpp ist schon lang genug. Gespeichert werden Fenstergeometrie (Größe, Lage,
// maximiert) und der Zustand von Docks und Werkzeugleisten (wo sie sitzen, welche sichtbar sind) in QSettings.
// Qt braucht dafür eindeutige Objektnamen; die vergibt nameLayoutParts. Im Live-Modus sind Docks absichtlich
// versteckt, dann wird nur die Geometrie gemerkt, sonst käme das Fenster beim nächsten Start ohne sie hoch.
#include <QByteArray>
#include <QDockWidget>
#include <QSettings>
#include <QStatusBar>

#include "main_window.hpp"

namespace whitehole::studio {

namespace {

constexpr int kLayoutVersion = 1;  // bei geänderten Docks erhöhen: Qt verwirft dann den alten Zustand

QSettings store() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

void MainWindow::nameLayoutParts() {
    const struct {
        QDockWidget* dock;
        const char* name;
    } docks[] = {{dock_, "dockParameter"}, {editorDock_, "dockNodes"}, {lookDock_, "dockLooks"},
                 {cueDock_, "dockCuelist"}, {liveDock_, "dockLive"}};
    for (const auto& entry : docks) {
        if (entry.dock != nullptr && entry.dock->objectName().isEmpty()) {
            entry.dock->setObjectName(QString::fromLatin1(entry.name));
        }
    }
}

void MainWindow::restoreWindowLayout() {
    nameLayoutParts();
    const QSettings s = store();
    const QByteArray geometry = s.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = s.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty()) {
        restoreState(state, kLayoutVersion);
    }
}

void MainWindow::saveWindowLayout() {
    nameLayoutParts();
    QSettings s = store();
    s.setValue(QStringLiteral("window/geometry"), saveGeometry());
    if (!liveMode_) {
        s.setValue(QStringLiteral("window/state"), saveState(kLayoutVersion));
    }
}

void MainWindow::resetWindowLayout() {
    QSettings s = store();
    s.remove(QStringLiteral("window/geometry"));
    s.remove(QStringLiteral("window/state"));
    statusBar()->showMessage(tr("Fensteranordnung zurückgesetzt: Sie gilt ab dem nächsten Start."), 8000);
}

}  // namespace whitehole::studio
