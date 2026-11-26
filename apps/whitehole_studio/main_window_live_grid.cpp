// main_window_live_grid.cpp: Rauchtest des festen Live-Rasters (Teil des Studio-Selbsttests, siehe main_window_live.cpp).
//
// Warum es diese Datei gibt: Das Raster mit frei belegbaren Feldern (docs/entwurf-live-raster.md) braucht eigene
// Prüfungen (Größe ändern, leeres Feld belegen, derselbe Look als Flash, Feld leeren), und main_window_live.cpp ist schon
// über dem Limit der Dateigröße. Qt läuft nur in der CI, dort fängt dieser Test Fehler auf.
#include <QMenu>

#include <memory>

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
    if (!livePanel_->clearCell(freeCell) || player_->cellLook(freeCell) >= 0 || player_->cellLook(0) != 0) {
        return QStringLiteral("Live: Ein Feld lässt sich nicht leeren");
    }
    return {};
}

}  // namespace whitehole::studio
