// main_window_dmx_monitor.cpp: Docks "DMX-Monitor" (Eingang, Schritt 1) und "DMX-Ausgangs-Monitor"
// (Ausgang, Schritt 2) im Hauptfenster, dazu die Rechtsklick-Zuordnung beider Docks (Schritt 3) und das
// "Wer hört zu"-Overlay des Eingangs (Schritt 4) - siehe CLAUDE.md, DMX-Patch/Monitor-Gitter.
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze; main_window_dmx.cpp ist Empfänger
// und programmweite DMX-Einstellungen (Master/Live), keine eigenständigen Docks. Hier steht nur das Anlegen
// der beiden Docks, ihr Eintrag im Menü "Ansicht" - dieselbe Stelle, an der cueDock_/lookDock_ entstehen
// (main_window_live.cpp) - und die Verbindung ihrer Rechtsklick-Signale zu LayoutPanel::addFromDialogAt
// (Ausgang) bzw. den drei neuen MainWindow::assign*-Methoden (main_window_dmx.cpp, Eingang). Beide Docks
// versteckt beim Start, wie cueDock_, damit sich am gewohnten Fenster nichts ändert; 512 Zellen wollen
// nicht ständig mitlaufen, nur weil es sie gibt.
#include <QDockWidget>

#include "dmx_monitor_panel.hpp"
#include "dmx_output_monitor_panel.hpp"
#include "layout_panel.hpp"
#include "main_window.hpp"

namespace whitehole::studio {

void MainWindow::buildDmxMonitor() {
    dmxMonitorPanel_ = new DmxMonitorPanel{library_->dmx, *player_, this};
    dmxMonitorDock_ = new QDockWidget{tr("DMX-Monitor"), this};
    dmxMonitorDock_->setWidget(dmxMonitorPanel_);
    addDockWidget(Qt::RightDockWidgetArea, dmxMonitorDock_);
    dmxMonitorDock_->hide();
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestMasterDmx, this, &MainWindow::assignMasterDmx);
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestLiveDmxSelect, this, &MainWindow::assignLiveDmxSelect);
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestLiveDmxFade, this, &MainWindow::assignLiveDmxFade);

    dmxOutputMonitorPanel_ = new DmxOutputMonitorPanel{player_->outputMonitor(), *player_, this};
    dmxOutputMonitorDock_ = new QDockWidget{tr("DMX-Ausgangs-Monitor"), this};
    dmxOutputMonitorDock_->setWidget(dmxOutputMonitorPanel_);
    addDockWidget(Qt::RightDockWidgetArea, dmxOutputMonitorDock_);
    dmxOutputMonitorDock_->hide();
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestManualPatch, layoutPanel_, &LayoutPanel::addFromDialogAt);
}

QString MainWindow::exerciseDmxMonitor() {
    // dmxMonitorPanel_ ist mit assignMasterDmx() verbunden (siehe buildDmxMonitor): sein Rauchtest löst
    // den Rechtsklick-Menüpunkt wirklich aus und setzt damit echt appSettings_.masterDmx - wie bei jedem
    // anderen Rauchtest, der Einstellungen anfasst (siehe exerciseSettings), muss der alte Stand zurück.
    const AppSettings before = appSettings_;
    const QString error = dmxMonitorPanel_->exercise();
    applySettings(before, false);
    return error;
}

QString MainWindow::exerciseDmxOutputMonitor() {
    // dmxOutputMonitorPanel_ ist mit LayoutPanel::addFromDialogAt verbunden (siehe buildDmxMonitor): sein
    // Rauchtest löst den Rechtsklick-Menüpunkt wirklich aus. Anders als bei assignMasterDmx öffnete das
    // echt den modalen "Fixture hinzufügen"-Dialog (dialog.exec()) - im Rauchtest ohne jemanden, der ihn
    // schließt, bliebe der Lauf stehen. Darum die Verbindung kurz lösen, statt nur den Stand danach
    // zurückzusetzen wie bei exerciseDmxMonitor.
    disconnect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestManualPatch, layoutPanel_, &LayoutPanel::addFromDialogAt);
    const QString error = dmxOutputMonitorPanel_->exercise();
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestManualPatch, layoutPanel_, &LayoutPanel::addFromDialogAt);
    return error;
}

}  // namespace whitehole::studio
