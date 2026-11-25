// player_live_grid.cpp: das feste Live-Raster im Player (Teil von Player, siehe player.hpp).
//
// Warum es diese Datei gibt: Das Raster (Felder mit Looks belegen, docs/entwurf-live-raster.md) braucht eigene kleine
// Zugriffe auf das Projekt; player_live.cpp bleibt so beim Speichern, Umbenennen und Löschen von Looks.
#include "whitehole/runtime/player.hpp"

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

Result<void> Player::setLiveGridSize(int columns, int rows) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (live_) {
        return Error{ErrorCode::InvalidArgument, "Im Live-Betrieb lässt sich die Rastergröße nicht ändern"};
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
    project::resizeLiveGrid(project_->liveGrid, columns, rows);
    modified_ = true;
    return {};
}

}  // namespace whitehole::runtime
