// main_window_dmx_monitor.cpp: Docks "DMX-Monitor" (Eingang, Schritt 1) und "DMX-Ausgangs-Monitor"
// (Ausgang, Schritt 2) im Hauptfenster (DMX-Patch/Monitor-Gitter, siehe CLAUDE.md).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze; main_window_dmx.cpp ist Empfänger
// und programmweite DMX-Einstellungen (Master/Live), keine eigenständigen Docks. Hier steht nur das Anlegen
// der beiden Docks und ihr Eintrag im Menü "Ansicht" - dieselbe Stelle, an der cueDock_/lookDock_ entstehen
// (main_window_live.cpp). Beide versteckt beim Start, wie cueDock_, damit sich am gewohnten Fenster nichts
// ändert; 512 Zellen wollen nicht ständig mitlaufen, nur weil es sie gibt.
#include <QDockWidget>

#include "dmx_monitor_panel.hpp"
#include "dmx_output_monitor_panel.hpp"
#include "main_window.hpp"

namespace whitehole::studio {

void MainWindow::buildDmxMonitor() {
    dmxMonitorPanel_ = new DmxMonitorPanel{library_->dmx, this};
    dmxMonitorDock_ = new QDockWidget{tr("DMX-Monitor"), this};
    dmxMonitorDock_->setWidget(dmxMonitorPanel_);
    addDockWidget(Qt::RightDockWidgetArea, dmxMonitorDock_);
    dmxMonitorDock_->hide();

    dmxOutputMonitorPanel_ = new DmxOutputMonitorPanel{player_->outputMonitor(), this};
    dmxOutputMonitorDock_ = new QDockWidget{tr("DMX-Ausgangs-Monitor"), this};
    dmxOutputMonitorDock_->setWidget(dmxOutputMonitorPanel_);
    addDockWidget(Qt::RightDockWidgetArea, dmxOutputMonitorDock_);
    dmxOutputMonitorDock_->hide();
}

QString MainWindow::exerciseDmxMonitor() { return dmxMonitorPanel_->exercise(); }
QString MainWindow::exerciseDmxOutputMonitor() { return dmxOutputMonitorPanel_->exercise(); }

}  // namespace whitehole::studio
