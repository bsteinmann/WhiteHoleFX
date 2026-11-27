// main_window_live_grid.cpp: Rauchtest des festen Live-Rasters (Teil des Studio-Selbsttests, siehe main_window_live.cpp).
//
// Warum es diese Datei gibt: Das Raster mit frei belegbaren Feldern (docs/entwurf-live-raster.md) braucht eigene
// Prüfungen (Größe ändern, leeres Feld belegen, derselbe Look als Flash, Feld leeren), und main_window_live.cpp ist schon
// über dem Limit der Dateigröße. Qt läuft nur in der CI, dort fängt dieser Test Fehler auf.
#include <QApplication>
#include <QCheckBox>
#include <QMenu>
#include <QPushButton>

#include <algorithm>
#include <memory>
#include <vector>

#include "live_panel.hpp"
#include "main_window.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

QString MainWindow::exerciseLiveGrid() {
    const project::LiveGrid& grid = player_->liveGrid();
    if (!grid.defined() || livePanel_->buttonCount() != grid.cellCount() || livePanel_->columns() != grid.columns) {
        return QStringLiteral("Live: Das Raster zeigt nicht die Felder des Projekts");
    }
    // Größe ändern: das Projekt folgt, belegte Felder behalten Zeile und Spalte
    const int columns = grid.columns;
    const int rows = grid.rows;
    livePanel_->setGrid(2, 3);
    if (livePanel_->columns() != 2 || livePanel_->rows() != 3 || livePanel_->buttonCount() != 6 || grid.columns != 2 || grid.rows != 3 ||
        player_->cellLook(0) != 0 || player_->cellLook(1) != 1) {
        return QStringLiteral("Live: Raster (Spalten, Zeilen) lässt sich nicht ändern");
    }
    livePanel_->setGrid(columns, rows);
    // Verkleinern per Eingabe: Gingen Looks verloren, fragt das Panel vorher nach (hier mit einer Funktion statt eines Dialogs)
    const std::vector<project::LiveCell> savedCells = grid.cells;  // der bestätigte Fall verwirft Looks: danach zurücksetzen
    int asked = 0;
    bool answer = false;
    livePanel_->setShrinkConfirm([&](int) {
        ++asked;
        return answer;
    });
    const int occupied = static_cast<int>(
        std::count_if(grid.cells.begin(), grid.cells.end(), [](const project::LiveCell& c) { return !c.empty(); }));
    if (occupied > 1) {  // auf 1 x 1 behält nur ein Feld seinen Look
        if (livePanel_->requestGrid(1, 1) || asked != 1 || livePanel_->columns() != columns || livePanel_->rows() != rows ||
            grid.columns != columns || grid.rows != rows) {
            livePanel_->setShrinkConfirm({});
            return QStringLiteral("Live: Raster verkleinern ohne Bestätigung verändert das Raster");
        }
        answer = true;
        if (!livePanel_->requestGrid(1, 1) || asked != 2 || grid.columns != 1 || grid.rows != 1) {
            livePanel_->setShrinkConfirm({});
            return QStringLiteral("Live: Raster verkleinern mit Bestätigung klappt nicht");
        }
        livePanel_->setGrid(columns, rows);
        for (std::size_t i = 0; i < savedCells.size(); ++i) {  // Ausgangszustand für die folgenden Prüfungen
            if (!player_->setLiveCell(static_cast<int>(i), savedCells[i]).ok()) {
                livePanel_->setShrinkConfirm({});
                return QStringLiteral("Live: Raster nach dem Verkleinern lässt sich nicht wiederherstellen");
            }
        }
        livePanel_->refresh();
    }
    livePanel_->setShrinkConfirm({});
    // Leeres Feld belegen: derselbe Look liegt dann zweimal, einmal als Flash
    const int freeCell = project::firstFreeLiveCell(grid);
    if (freeCell < 0 || player_->cellLook(freeCell) >= 0 || livePanel_->trigger(freeCell)) {
        return QStringLiteral("Live: Ein leeres Feld lässt sich auslösen oder fehlt");
    }
    if (const std::unique_ptr<QMenu> menu{livePanel_->lookMenu(freeCell)}; menu->actions().size() != 1) {
        return QStringLiteral("Live: Das Menü eines leeren Feldes bietet nicht nur \"Look hierher legen\" an");
    }
    const std::string name = player_->looks()[0].name;
    if (!livePanel_->assignLook(freeCell, name) || player_->cellLook(freeCell) != 0 || livePanel_->buttonCount() != grid.cellCount()) {
        return QStringLiteral("Live: Ein Look lässt sich nicht auf ein leeres Feld legen");
    }
    if (!livePanel_->setLookTrigger(freeCell, project::LookTrigger::Flash, 1.5) ||
        grid.cells[static_cast<std::size_t>(freeCell)].trigger != project::LookTrigger::Flash ||
        grid.cells[0].trigger != project::LookTrigger::Toggle) {
        return QStringLiteral("Live: Flash gehört nicht zum einzelnen Feld, sondern zum Look");
    }
    // Overlay: der Haken setzt das Feld, das Drücken legt den Look über den laufenden, Loslassen nimmt ihn wieder weg
    const auto cellIndex = static_cast<std::size_t>(freeCell);
    QCheckBox* overlay = livePanel_->overlayCheck(freeCell);
    if (overlay == nullptr || !overlay->isEnabled()) {
        return QStringLiteral("Live: Der Overlay-Haken fehlt oder ist bei einem Flash-Feld gesperrt");
    }
    overlay->click();
    if (!grid.cells[cellIndex].flashOverlay || !overlay->isChecked()) {
        return QStringLiteral("Live: Der Overlay-Haken stellt das Feld nicht auf Overlay");
    }
    if (const std::unique_ptr<QMenu> menu{livePanel_->lookMenu(freeCell)}; menu->actions().isEmpty()) {
        return QStringLiteral("Live: Das Menü eines Overlay-Feldes ist leer");
    }
    const int before = player_->liveStatus().current;
    if (!livePanel_->press(freeCell) || !player_->liveStatus().overlay || player_->liveStatus().current != before) {
        livePanel_->release(freeCell);
        return QStringLiteral("Live: Ein Overlay-Feld legt sich nicht über den laufenden Look");
    }
    livePanel_->release(freeCell);
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    if (!livePanel_->setLookTrigger(freeCell, project::LookTrigger::Toggle, std::nullopt) || grid.cells[cellIndex].flashOverlay ||
        overlay->isEnabled()) {
        return QStringLiteral("Live: Umschalten setzt Overlay nicht zurück");
    }
    if (!livePanel_->clearCell(freeCell) || player_->cellLook(freeCell) >= 0 || player_->cellLook(0) != 0) {
        return QStringLiteral("Live: Ein Feld lässt sich nicht leeren");
    }
    // TAP: ein Tipp erreicht die Tempo-Uhr, die Taste zeigt das Tempo
    if (livePanel_->tapButton() == nullptr) {
        return QStringLiteral("Live: Die Taste TAP fehlt");
    }
    livePanel_->tapButton()->click();
    tick();
    if (!livePanel_->tapButton()->text().contains(QStringLiteral("BPM"))) {
        return QStringLiteral("Live: Die Taste TAP zeigt kein Tempo");
    }
    return {};
}

}  // namespace whitehole::studio
