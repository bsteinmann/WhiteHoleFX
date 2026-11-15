// main_window_dmx_monitor.cpp: Dock "DMX-Monitor" im Hauptfenster (Schritt 1 des DMX-Patch/Monitor-Gitters).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze; main_window_dmx.cpp ist Empfänger
// und programmweite DMX-Einstellungen (Master/Live), kein eigenständiges Dock. Hier steht nur das Anlegen
// des Docks und sein Eintrag im Menü "Ansicht" - dieselbe Stelle, an der cueDock_/lookDock_ entstehen
// (main_window_live.cpp). Versteckt beim Start, wie cueDock_, damit sich am gewohnten Fenster nichts ändert.
#include <QDockWidget>

#include "dmx_monitor_panel.hpp"
#include "main_window.hpp"

namespace whitehole::studio {

void MainWindow::buildDmxMonitor() {
    dmxMonitorPanel_ = new DmxMonitorPanel{library_->dmx, this};
    dmxMonitorDock_ = new QDockWidget{tr("DMX-Monitor"), this};
    dmxMonitorDock_->setWidget(dmxMonitorPanel_);
    addDockWidget(Qt::RightDockWidgetArea, dmxMonitorDock_);
    dmxMonitorDock_->hide();  // erst auf Wunsch (Ansicht → DMX-Monitor); 512 Zellen wollen nicht ständig mitlaufen
}

QString MainWindow::exerciseDmxMonitor() { return dmxMonitorPanel_->exercise(); }

}  // namespace whitehole::studio
