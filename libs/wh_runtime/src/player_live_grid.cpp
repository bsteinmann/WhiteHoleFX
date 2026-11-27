// player_live_grid.cpp: das feste Live-Raster im Player (Teil von Player, siehe player.hpp).
//
// Warum es diese Datei gibt: Das Raster (Felder mit Looks belegen, docs/entwurf-live-raster.md) braucht eigene kleine
// Zugriffe auf das Projekt; player_live.cpp bleibt so beim Speichern, Umbenennen und Löschen von Looks.
#include "whitehole/runtime/player.hpp"

#include <cmath>

#include "whitehole/runtime/live_look.hpp"

namespace whitehole::runtime {

const project::LiveGrid& Player::liveGrid() const {
    static const project::LiveGrid kNone;
    return project_ ? project_->liveGrid : kNone;
}

Result<void> Player::ensureLiveGrid(int columns, int rows) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (project_->liveGrid.defined()) {
        return {};
    }
    project::LiveGrid grid = project::makeLiveGridFromLooks(project_->looks, columns, rows);
    if (!grid.defined()) {
        return Error{ErrorCode::InvalidArgument, "Das Live-Raster lässt sich so nicht anlegen (Spalten 1 bis 16, höchstens 16 Zeilen)"};
    }
    project_->liveGrid = std::move(grid);  // bewusst kein modified_: die Datei ändert sich erst beim nächsten Speichern
    return {};
}

Result<void> Player::setLiveCell(int cell, project::LiveCell value) {
    if (!project_ || !project_->liveGrid.defined()) {
        return Error{ErrorCode::InvalidArgument, "Das Projekt hat noch kein Live-Raster"};
    }
    project::LiveGrid& grid = project_->liveGrid;
    if (cell < 0 || static_cast<std::size_t>(cell) >= grid.cells.size()) {
        return Error{ErrorCode::NotFound, "Es gibt kein Feld Nummer " + std::to_string(cell + 1)};
    }
    if (value.empty()) {
        value = project::LiveCell{};
    } else if (findLook(project_->looks, value.look) < 0) {
        return Error{ErrorCode::NotFound, "Es gibt keinen Look \"" + value.look + "\""};
    }
    for (const std::optional<double>& seconds : {value.flashInSeconds, value.returnSeconds}) {
        if (seconds && !(std::isfinite(*seconds) && *seconds >= 0.0 && *seconds <= project::kMaxFadeSeconds)) {
            return Error{ErrorCode::InvalidArgument, "Die Zeit liegt zwischen 0 und 600 Sekunden"};
        }
    }
    if (value.trigger != project::LookTrigger::Flash) {
        value.flashInSeconds.reset();  // ohne Flash gibt es kein Einblenden/Zurückblenden beim Drücken/Loslassen
        value.returnSeconds.reset();
    }
    project::LiveCell& target = grid.cells[static_cast<std::size_t>(cell)];
    if (target.look == value.look && target.trigger == value.trigger && target.flashInSeconds == value.flashInSeconds &&
        target.returnSeconds == value.returnSeconds) {
        return {};
    }
    target = std::move(value);
    clearFlash();  // ein gehaltenes Flash kennt das alte Feld nicht mehr
    modified_ = true;
    return {};
}

int Player::liveGridCellsLost(int columns, int rows) const {
    if (!project_ || !project_->liveGrid.defined() || columns < 1 || columns > project::kMaxLiveGridSide || rows < 1 ||
        rows > project::kMaxLiveGridSide) {
        return 0;
    }
    return project::liveGridCellsLost(project_->liveGrid, columns, rows);
}

Result<void> Player::setLiveGridSize(int columns, int rows, bool allowDrop) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (!project_->liveGrid.defined()) {
        return Error{ErrorCode::InvalidArgument, "Das Projekt hat noch kein Live-Raster"};
    }
    if (columns < 1 || columns > project::kMaxLiveGridSide || rows < 1 || rows > project::kMaxLiveGridSide) {
        return Error{ErrorCode::InvalidArgument, "Das Raster hat 1 bis 16 Spalten und Zeilen"};
    }
    if (columns == project_->liveGrid.columns && rows == project_->liveGrid.rows) {
        return {};
    }
    if (!allowDrop) {
        if (const int lost = project::liveGridCellsLost(project_->liveGrid, columns, rows); lost > 0) {
            return Error{ErrorCode::InvalidArgument,
                         std::to_string(lost) + " belegte Felder passen nicht mehr ins Raster"};
        }
    }
    project::resizeLiveGrid(project_->liveGrid, columns, rows);
    clearFlash();  // gehaltene Felder können verschwunden oder verschoben sein
    modified_ = true;
    return {};
}

}  // namespace whitehole::runtime
