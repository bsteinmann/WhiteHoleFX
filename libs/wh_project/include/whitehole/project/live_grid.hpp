// live_grid.hpp: das feste Raster des Live-Modus mit frei belegbaren Feldern.
//
// Warum es diese Datei gibt (Wunsch vom 25.11., docs/entwurf-live-raster.md): Im Live-Fenster liegt nicht mehr jeder
// gespeicherte Look automatisch auf einem Knopf. Ein festes Raster (Spalten x Zeilen) hat Felder, die man mit Looks
// belegt oder leer lässt, und derselbe Look darf in mehreren Feldern liegen, z. B. einmal als Flash und einmal zum
// Umschalten. Darum gehören Flash und die Zeiten zum *Feld*, nicht mehr zum Look. Das Modell hier ist nur Daten
// (Projektdatei, Player und Oberfläche teilen es); der Ablauf steht in wh_runtime.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::project {

/// Wie ein Look im Live-Betrieb ausgelöst wird (docs/entwurf-midi.md, Abschnitt 5).
///   Toggle: ein Druck blendet dauerhaft um (wie bisher).
///   Flash:  der Look läuft nur, solange gehalten; beim Loslassen zurück zum vorherigen. Hat Vorrang vor "Mit GO".
enum class LookTrigger : std::uint8_t { Toggle, Flash };

inline constexpr int kMaxLiveGridSide = 16;  // Spalten und Zeilen, je 1 bis 16

/// Ein Feld des Rasters. Leerer `look` = Feld nicht belegt (die übrigen Angaben zählen dann nicht).
struct LiveCell {
    std::string look;  // Name eines Looks der Datei
    LookTrigger trigger{LookTrigger::Toggle};
    /// Nur Flash: Zeit für das Einblenden beim Drücken. Leer = die Überblendzeit des Looks.
    std::optional<double> flashInSeconds;
    /// Nur Flash: Zeit für das Zurückblenden beim Loslassen. Leer = die Überblendzeit des Looks.
    std::optional<double> returnSeconds;

    [[nodiscard]] bool empty() const noexcept { return look.empty(); }
};

/// Das Raster: `cells` hat genau `columns * rows` Einträge, zeilenweise (Feld = zeile * columns + spalte).
/// `columns == 0` = kein Raster definiert (ältere Projekte).
struct LiveGrid {
    int columns{0};
    int rows{0};
    std::vector<LiveCell> cells;

    [[nodiscard]] bool defined() const noexcept { return columns > 0 && rows > 0; }
    [[nodiscard]] int cellCount() const noexcept { return columns * rows; }
};

/// Leeres Raster in der Größe `columns` x `rows` (je auf 1..16 begrenzt).
[[nodiscard]] LiveGrid makeEmptyLiveGrid(int columns, int rows);

/// Neue Größe; belegte Felder behalten ihre Zeile und Spalte, Felder außerhalb fallen weg (die Looks bleiben im Projekt).
void resizeLiveGrid(LiveGrid& grid, int columns, int rows);

/// Erstes leeres Feld (zeilenweise) oder -1.
[[nodiscard]] int firstFreeLiveCell(const LiveGrid& grid) noexcept;

/// Ein Look wurde umbenannt: alle Felder mit dem alten Namen zeigen auf den neuen.
void renameLookInLiveGrid(LiveGrid& grid, std::string_view oldName, const std::string& newName);

/// Ein Look wurde gelöscht: alle Felder mit diesem Namen werden leer.
void clearLookInLiveGrid(LiveGrid& grid, std::string_view name);

}  // namespace whitehole::project
