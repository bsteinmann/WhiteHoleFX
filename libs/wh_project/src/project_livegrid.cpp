// project_livegrid.cpp: `livegrid`- und `cell`-Zeilen der Projektdatei (siehe project_livegrid.hpp).
#include "project_livegrid.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace whitehole::project::detail {

namespace {

double seconds(Reader& r, std::string_view what) {
    const double value = r.numberD(what);
    if (r.ok() && !(value >= 0.0 && value <= kMaxFadeSeconds)) {
        r.fail(std::string{what} + " muss zwischen 0 und " + std::to_string(static_cast<int>(kMaxFadeSeconds)) +
               " Sekunden liegen");
    }
    return value;
}

}  // namespace

LiveGrid parseLiveGridHeader(Reader& r, const LiveGrid& existing) {
    if (existing.defined()) {
        r.fail("Es darf nur eine livegrid-Zeile geben");
        return {};
    }
    const std::uint32_t columns = r.whole("Spalten", 4096);
    const std::uint32_t rows = r.whole("Zeilen", 4096);
    r.expectEnd();
    if (!r.ok()) {
        return {};
    }
    const auto side = static_cast<std::uint32_t>(kMaxLiveGridSide);
    if (columns < 1 || columns > side || rows < 1 || rows > side) {
        r.fail("Das Raster hat 1 bis " + std::to_string(kMaxLiveGridSide) + " Spalten und Zeilen");
        return {};
    }
    return makeEmptyLiveGrid(static_cast<int>(columns), static_cast<int>(rows));
}

void parseLiveCell(Reader& r, LiveGrid& grid) {
    if (!grid.defined()) {
        r.fail("\"cell\" braucht vorher eine livegrid-Zeile");
        return;
    }
    const std::uint32_t row = r.whole("Zeile", 4096);
    const std::uint32_t column = r.whole("Spalte", 4096);
    if (!r.ok()) {
        return;
    }
    if (row < 1 || column < 1 || row > static_cast<std::uint32_t>(grid.rows) || column > static_cast<std::uint32_t>(grid.columns)) {
        r.fail("Das Feld " + std::to_string(row) + "/" + std::to_string(column) + " liegt außerhalb des Rasters (" +
               std::to_string(grid.rows) + " Zeilen, " + std::to_string(grid.columns) + " Spalten)");
        return;
    }
    LiveCell cell;
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Einstellung");
        if (!r.ok()) {
            break;
        }
        if (key == "look") {
            cell.look = r.word("Lookname");
        } else if (key == "flash") {
            cell.trigger = LookTrigger::Flash;
        } else if (key == "in") {
            cell.flashInSeconds = seconds(r, "in");
        } else if (key == "return") {
            cell.returnSeconds = seconds(r, "return");
        } else {
            r.fail("Unbekannte Einstellung \"" + key + "\" in der cell-Zeile");
        }
    }
    if (!r.ok()) {
        return;
    }
    if (cell.look.empty()) {
        r.fail("Die cell-Zeile braucht einen look");
        return;
    }
    if (cell.trigger != LookTrigger::Flash && (cell.flashInSeconds || cell.returnSeconds)) {
        r.fail("\"in\" und \"return\" gibt es nur zusammen mit \"flash\"");
        return;
    }
    LiveCell& target = grid.cells[static_cast<std::size_t>((row - 1) * static_cast<std::uint32_t>(grid.columns) + (column - 1))];
    if (!target.empty()) {
        r.fail("Das Feld " + std::to_string(row) + "/" + std::to_string(column) + " ist schon belegt");
        return;
    }
    target = std::move(cell);
}

std::string formatLiveGrid(const LiveGrid& grid) {
    if (!grid.defined()) {
        return {};
    }
    std::ostringstream out;
    out << "livegrid " << grid.columns << ' ' << grid.rows << "\n";
    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.columns; ++col) {
            const LiveCell& cell = grid.cells[static_cast<std::size_t>(row * grid.columns + col)];
            if (cell.empty()) {
                continue;
            }
            out << "cell " << row + 1 << ' ' << col + 1 << " look " << quoteWord(cell.look);
            if (cell.trigger == LookTrigger::Flash) {
                out << " flash";
                if (cell.flashInSeconds) {
                    out << " in " << formatDouble(*cell.flashInSeconds);
                }
                if (cell.returnSeconds) {
                    out << " return " << formatDouble(*cell.returnSeconds);
                }
            }
            out << "\n";
        }
    }
    return out.str();
}

Result<void> checkLiveGrid(const LiveGrid& grid, const std::vector<Look>& looks) {
    if (!grid.defined()) {
        return {};
    }
    if (grid.columns > kMaxLiveGridSide || grid.rows > kMaxLiveGridSide ||
        grid.cells.size() != static_cast<std::size_t>(grid.cellCount())) {
        return Error{ErrorCode::InvalidArgument, "Das Live-Raster hat eine ungültige Größe"};
    }
    for (std::size_t i = 0; i < grid.cells.size(); ++i) {
        const LiveCell& cell = grid.cells[i];
        if (cell.empty()) {
            continue;
        }
        const bool found = std::any_of(looks.begin(), looks.end(), [&](const Look& l) { return l.name == cell.look; });
        if (!found) {
            return Error{ErrorCode::NotFound, "Live-Raster, Feld " + std::to_string(i / static_cast<std::size_t>(grid.columns) + 1) +
                                                  "/" + std::to_string(i % static_cast<std::size_t>(grid.columns) + 1) +
                                                  ": Der Look \"" + cell.look + "\" ist nicht in der Datei"};
        }
    }
    return {};
}

}  // namespace whitehole::project::detail
