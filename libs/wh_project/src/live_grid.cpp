// live_grid.cpp: Raster-Hilfen für den Live-Modus (siehe live_grid.hpp) und die Übernahme alter Projekte.
#include "whitehole/project/live_grid.hpp"

#include <algorithm>

#include "whitehole/project/project.hpp"

namespace whitehole::project {

LiveGrid makeEmptyLiveGrid(int columns, int rows) {
    LiveGrid grid;
    grid.columns = std::clamp(columns, 1, kMaxLiveGridSide);
    grid.rows = std::clamp(rows, 1, kMaxLiveGridSide);
    grid.cells.resize(static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows));
    return grid;
}

void resizeLiveGrid(LiveGrid& grid, int columns, int rows) {
    LiveGrid result = makeEmptyLiveGrid(columns, rows);
    for (int row = 0; row < std::min(grid.rows, result.rows); ++row) {
        for (int col = 0; col < std::min(grid.columns, result.columns); ++col) {
            result.cells[static_cast<std::size_t>(row * result.columns + col)] =
                std::move(grid.cells[static_cast<std::size_t>(row * grid.columns + col)]);
        }
    }
    grid = std::move(result);
}

int firstFreeLiveCell(const LiveGrid& grid) noexcept {
    for (std::size_t i = 0; i < grid.cells.size(); ++i) {
        if (grid.cells[i].empty()) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void fillFlashDefaults(LiveCell& cell) noexcept {
    if (!cell.flashInSeconds) {
        cell.flashInSeconds = kDefaultFlashSeconds;
    }
    if (!cell.returnSeconds) {
        cell.returnSeconds = kDefaultFlashSeconds;
    }
}

void renameLookInLiveGrid(LiveGrid& grid, std::string_view oldName, const std::string& newName) {
    for (LiveCell& cell : grid.cells) {
        if (cell.look == oldName) {
            cell.look = newName;
        }
    }
}

void clearLookInLiveGrid(LiveGrid& grid, std::string_view name) {
    for (LiveCell& cell : grid.cells) {
        if (cell.look == name) {
            cell = LiveCell{};
        }
    }
}

LiveGrid makeLiveGridFromLooks(const std::vector<Look>& looks, int columns, int rows) {
    if (columns < 1 || columns > kMaxLiveGridSide) {
        return {};
    }
    const int needed = (static_cast<int>(looks.size()) + columns - 1) / columns;
    if (needed > kMaxLiveGridSide) {
        return {};  // passt nicht in 16 Zeilen: der Aufrufer wählt dann mehr Spalten
    }
    LiveGrid grid = makeEmptyLiveGrid(columns, std::max(rows, needed));
    for (std::size_t i = 0; i < looks.size(); ++i) {
        LiveCell& cell = grid.cells[i];
        cell.look = looks[i].name;
        cell.trigger = looks[i].trigger;
        cell.flashInSeconds = looks[i].flashInSeconds;
        cell.returnSeconds = looks[i].returnSeconds;
    }
    return grid;
}

}  // namespace whitehole::project
