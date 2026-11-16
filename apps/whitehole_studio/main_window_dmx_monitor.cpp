// main_window_dmx_monitor.cpp: das Fenster "DMX-Monitor" (Eingang, Schritt 1, und Ausgang, Schritt 2, als
// Reiter) im Hauptfenster, dazu die Rechtsklick-Zuordnung beider Reiter (Schritt 3), das "Wer hört zu"-
// Overlay beider Reiter (Schritt 4 plus die nachträglich freigegebene Erweiterung für die Ausgabe) und die
// Rechtsklick-Bearbeitung/-Entfernung eines schon bestehenden Ausgangs-Patches (ebenfalls nachträglich
// freigegeben) - siehe CLAUDE.md, DMX-Patch/Monitor-Gitter.
//
// Vereinheitlichte Node-Adressierung (15.11., Richtung passend zum Monitor): beide Reiter haben jetzt ein
// gleichnamiges Rechtsklick-Untermenü "Node" (Eingang: umbenannt von "DMX Out", siehe dmx_monitor_panel.hpp;
// Ausgang: neu, siehe dmx_output_monitor_panel.hpp) - bewusst zwei getrennte Signale/Verbindungen
// (requestPatchNode -> MainWindow::patchDmxValueNode für Lese-Nodes, requestPatchOutputNode ->
// MainWindow::patchDmxOutputValueNode für Schreib-Nodes), weil Empfangs- und Sende-Adressraum
// unterschiedliche Dinge sind. Fixtures bleiben bewusst außen vor beiden Untermenüs - sie patchen
// ausschließlich über die bestehende Fixture-Patch-Tabelle ("Neues Fixture hier patchen…" gab es hier
// früher zusätzlich auf der freien Zelle des Ausgangs-Monitors, vom Nutzer wieder entfernt).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze; main_window_dmx.cpp ist Empfänger
// und programmweite DMX-Einstellungen (Master/Live), kein eigenständiges Fenster. Hier steht nur das
// Anlegen des Fensters (nachträglich freigegebene Erweiterung: eigenes Fenster statt zweier Docks, nach
// demselben Muster wie Stage Config, siehe dmx_monitor_window.hpp - der Node-Editor und die Parameter
// sollen den Platz nicht an zwei Monitor-Docks abgeben müssen, die man selten gleichzeitig mit dem Bauen
// von Effekten braucht), sein Menüeintrag in "Ansicht" und die Verbindung der Rechtsklick-Signale beider
// Reiter zu StageSetupWindow::openOutputFixture/openOutputPool (Ausgang, "bearbeiten") bzw. dem Entfernen
// eines Pool-Patches direkt hier ("Patch aufheben", requestRemovePoolPatch, nach demselben Filter-Muster
// wie DmxPatchTable::removeSelectedPatch) sowie den MainWindow::assign*/patch*-Methoden (main_window_dmx.cpp,
// beide Richtungen). Anders als Stage Config schließt der Live-Modus dieses Fenster nicht (siehe
// dmx_monitor_window.hpp) - beim Start trotzdem versteckt, wie cueDock_, damit sich am gewohnten Fenster
// nichts ändert; 512 Zellen wollen nicht ständig mitlaufen, nur weil es sie gibt (siehe MainWindow::tick,
// das refresh() nur bei sichtbarem Reiter ruft).
#include <QAction>
#include <QKeySequence>

#include <algorithm>
#include <vector>

#include "dmx_monitor_panel.hpp"
#include "dmx_monitor_window.hpp"
#include "dmx_output_monitor_panel.hpp"
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
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestEditFixturePatch, this,
            [this](mapping::FixtureId id) { stageSetup_->openOutputFixture(id); });
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestEditPoolPatch, this,
            [this](std::uint16_t poolChannel) { stageSetup_->openOutputPool(poolChannel); });
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestRemovePoolPatch, this, [this](std::uint16_t poolChannel) {
        // "Patch aufheben" (vom Nutzer gewünscht: "rechtklick -> node -> patch aufheben") - gleiches
        // Filter-Muster wie DmxPatchTable::removeSelectedPatch, nur nach poolChannel statt nach Zeilenindex.
        if (player_->project() == nullptr) {
            return;
        }
        std::vector<output::DmxChannelPatch> patches = player_->project()->dmxPatches;
        std::erase_if(patches, [poolChannel](const output::DmxChannelPatch& p) { return p.poolChannel == poolChannel; });
        player_->setDmxPatches(std::move(patches));
    });
    connect(dmxOutputMonitorPanel_, &DmxOutputMonitorPanel::requestPatchOutputNode, this, &MainWindow::patchDmxOutputValueNode);

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
    // dmxOutputMonitorPanel_ ist mit den "bearbeiten"-Verbindungen, requestRemovePoolPatch ("Patch
    // aufheben") und requestPatchOutputNode (Untermenü "Node") verbunden (siehe buildDmxMonitor): sein
    // Rauchtest löst die Menüpunkte wirklich aus, keiner davon öffnet einen modalen Dialog (anders als
    // früher "Neues Fixture hier patchen…", das darum eine eigene Disconnect/Reconnect-Behandlung brauchte -
    // seit der Nutzer diesen Menüpunkt entfernen ließ, entfällt das hier). "bearbeiten" öffnet nur das
    // längst unmodale Stage-Config-Fenster auf der Seite Ausgabe - danach aber wieder verstecken, wie
    // exerciseStageSetup() es vorher schon einmal versteckt verlassen hat.
    const QString error = dmxOutputMonitorPanel_->exercise();
    stageSetup_->hide();
    // Vor exerciseDmxMonitor() war das Fenster versteckt (siehe buildDmxMonitor) - wieder in diesen Stand
    // zurück, damit sich am gewohnten Fenster nach dem Rauchtest nichts ändert.
    dmxMonitorWindow_->hide();
    return error;
}

}  // namespace whitehole::studio
