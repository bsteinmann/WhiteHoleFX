// main_window_dmx_monitor.cpp: das Fenster "DMX-Monitor" (Eingang, Schritt 1, und Ausgang, Schritt 2, als
// Reiter) im Hauptfenster, dazu die Rechtsklick-Zuordnung beider Reiter (Schritt 3), das "Wer hört zu"-
// Overlay beider Reiter (Schritt 4 plus die nachträglich freigegebene Erweiterung für die Ausgabe) und die
// Rechtsklick-Bearbeitung eines schon bestehenden Ausgangs-Patches (ebenfalls nachträglich freigegeben) -
// siehe CLAUDE.md, DMX-Patch/Monitor-Gitter.
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze; main_window_dmx.cpp ist Empfänger
// und programmweite DMX-Einstellungen (Master/Live), kein eigenständiges Fenster. Hier steht nur das
// Anlegen des Fensters (nachträglich freigegebene Erweiterung: eigenes Fenster statt zweier Docks, nach
// demselben Muster wie Stage Config, siehe dmx_monitor_window.hpp - der Node-Editor und die Parameter
// sollen den Platz nicht an zwei Monitor-Docks abgeben müssen, die man selten gleichzeitig mit dem Bauen
// von Effekten braucht), sein Menüeintrag in "Ansicht" und die Verbindung der Rechtsklick-Signale beider
// Reiter zu LayoutPanel::addFromDialogAt bzw. StageSetupWindow::openOutputFixture/openOutputPool (Ausgang)
// sowie den drei MainWindow::assign*-Methoden (main_window_dmx.cpp, Eingang). Anders als Stage Config
// schließt der Live-Modus dieses Fenster nicht (siehe dmx_monitor_window.hpp) - beim Start trotzdem
// versteckt, wie cueDock_, damit sich am gewohnten Fenster nichts ändert; 512 Zellen wollen nicht ständig
// mitlaufen, nur weil es sie gibt (siehe MainWindow::tick, das refresh() nur bei sichtbarem Reiter ruft).
#include <QAction>
#include <QKeySequence>

#include "dmx_monitor_panel.hpp"
#include "dmx_monitor_window.hpp"
#include "dmx_output_monitor_panel.hpp"
#include "layout_panel.hpp"
#include "main_window.hpp"
#include "stage_setup_window.hpp"

namespace whitehole::studio {

void MainWindow::buildDmxMonitor() {
    dmxMonitorPanel_ = new DmxMonitorPanel{library_->dmx, *player_, this};
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestMasterDmx, this, &MainWindow::assignMasterDmx);
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestLiveDmxSelect, this, &MainWindow::assignLiveDmxSelect);
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestLiveDmxFade, this, &MainWindow::assignLiveDmxFade);
    connect(dmxMonitorPanel_, &DmxMonitorPanel::requestPatchNode, this, &MainWindow::patchDmxValueNode);

    dmxOutputMonitorPanel_ = new DmxOutputMonitorPanel{player_->outputMonitor(), *player_, this};
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestManualPatch, layoutPanel_, &LayoutPanel::addFromDialogAt);
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestEditFixturePatch, this,
            [this](mapping::FixtureId id) { stageSetup_->openOutputFixture(id); });
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestEditPoolPatch, this,
            [this](std::uint16_t poolChannel) { stageSetup_->openOutputPool(poolChannel); });

    dmxMonitorWindow_ = new DmxMonitorWindow{dmxMonitorPanel_, dmxOutputMonitorPanel_, this};
    dmxMonitorWindow_->hide();

    dmxMonitorAction_ = new QAction{tr("&DMX-Monitor …"), this};
    dmxMonitorAction_->setShortcut(QKeySequence{Qt::Key_F6});
    dmxMonitorAction_->setToolTip(tr("Empfangene und gesendete DMX-Kanäle einsehen und direkt patchen (eigenes Fenster)"));
    connect(dmxMonitorAction_, &QAction::triggered, this, [this]() { dmxMonitorWindow_->open(DmxMonitorWindow::Page::Input); });
}

QString MainWindow::exerciseDmxMonitor() {
    if (dmxMonitorWindow_ == nullptr || dmxMonitorWindow_->pageCount() != 2) {
        return QStringLiteral("das Fenster DMX-Monitor hat nicht die Reiter Eingang und Ausgang");
    }
    if (dmxMonitorWindow_->isVisible()) {
        return QStringLiteral("DMX-Monitor ist schon offen");
    }
    dmxMonitorAction_->trigger();
    if (!dmxMonitorWindow_->isVisible() || !dmxMonitorWindow_->isWindow() ||
        dmxMonitorWindow_->page() != DmxMonitorWindow::Page::Input) {
        return QStringLiteral("DMX-Monitor öffnet sich nicht als eigenes Fenster auf dem Reiter Eingang");
    }
    // dmxMonitorPanel_ ist mit assignMasterDmx() verbunden (siehe buildDmxMonitor): sein Rauchtest löst
    // den Rechtsklick-Menüpunkt wirklich aus und setzt damit echt appSettings_.masterDmx - wie bei jedem
    // anderen Rauchtest, der Einstellungen anfasst (siehe exerciseSettings), muss der alte Stand zurück.
    const AppSettings before = appSettings_;
    const QString error = dmxMonitorPanel_->exercise();
    applySettings(before, false);
    return error;
}

QString MainWindow::exerciseDmxOutputMonitor() {
    dmxMonitorWindow_->open(DmxMonitorWindow::Page::Output);
    if (dmxMonitorWindow_->page() != DmxMonitorWindow::Page::Output) {
        return QStringLiteral("DMX-Monitor zeigt den Reiter Ausgang nicht");
    }
    // dmxOutputMonitorPanel_ ist mit LayoutPanel::addFromDialogAt verbunden (siehe buildDmxMonitor): sein
    // Rauchtest löst den Rechtsklick-Menüpunkt wirklich aus. Anders als bei assignMasterDmx öffnete das
    // echt den modalen "Fixture hinzufügen"-Dialog (dialog.exec()) - im Rauchtest ohne jemanden, der ihn
    // schließt, bliebe der Lauf stehen. Darum die Verbindung kurz lösen, statt nur den Stand danach
    // zurückzusetzen wie bei exerciseDmxMonitor.
    disconnect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestManualPatch, layoutPanel_, &LayoutPanel::addFromDialogAt);
    // Die beiden "bearbeiten"-Verbindungen bleiben dagegen bestehen: sie öffnen nur das längst unmodale
    // Stage-Config-Fenster auf der Seite Ausgabe, kein Dialog, der den Rauchtest blockieren könnte. Nach dem
    // Rauchtest aber wieder verstecken - exerciseStageSetup() hat es vorher schon einmal versteckt verlassen.
    const QString error = dmxOutputMonitorPanel_->exercise();
    stageSetup_->hide();
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestManualPatch, layoutPanel_, &LayoutPanel::addFromDialogAt);
    // Vor exerciseDmxMonitor() war das Fenster versteckt (siehe buildDmxMonitor) - wieder in diesen Stand
    // zurück, damit sich am gewohnten Fenster nach dem Rauchtest nichts ändert.
    dmxMonitorWindow_->hide();
    return error;
}

}  // namespace whitehole::studio
