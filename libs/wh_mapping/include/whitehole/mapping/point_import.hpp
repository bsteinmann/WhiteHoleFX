// point_import.hpp: LED-Positionen aus CSV- oder JSON-Dateien einlesen.
//
// Warum es diese Datei gibt: Wer eine Installation schon geplant hat (CAD, Tabellenkalkulation, ein Skript,
// das Positionen ausrechnet), soll die Koordinaten nicht von Hand eintippen. Der Import liefert die Punkte
// einer Punktwolke (PointsShape) in Kabelreihenfolge: die erste Zeile ist die erste LED am Kabel.
//
// Aufgabe: Text -> Positionen, ohne Dateizugriff (den macht der Aufrufer). Die Formate:
//   CSV   eine LED je Zeile: "x,y" oder "x,y,z". Trenner , ; Tabulator oder Leerraum (aus der ersten
//         Datenzeile erkannt). Bei ; und Tabulator ist ein Dezimalkomma erlaubt (deutsches Excel).
//         Eine Kopfzeile (erste Zeile mit Text) ordnet die Spalten über die Namen x, y, z; weitere
//         Spalten (z. B. "index") werden ignoriert. Zeilen mit # und leere Zeilen werden übersprungen.
//   JSON  eine Liste von [x,y] / [x,y,z] oder von Objekten {"x":..,"y":..,"z":..}; auch als Wert eines
//         Feldes "points", "leds", "pixels" oder "positions" in einem Objekt.
// Danach: `flipY` (Bilder und manche CAD-Programme zählen y nach unten), `scale` (z. B. Millimeter ->
// Zentimeter), `toOrigin` (kleinste Koordinate je Achse auf 0; die Lage im Raum bestimmt der Ursprung des Fixtures).
// Abhängigkeiten: wh_core.
#pragma once

#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"

namespace whitehole::mapping {

struct ImportOptions {
    float scale{1.0F};   // Faktor auf alle Koordinaten (> 0)
    bool flipY{false};   // y -> -y (vor toOrigin)
    bool toOrigin{true}; // kleinste Koordinate je Achse auf 0 schieben
};

/// Fehler: InvalidArgument mit Zeilennummer bzw. Position; leere Dateien und mehr als
/// kMaxLedsPerFixture Punkte sind Fehler.
[[nodiscard]] Result<std::vector<Vec3>> parsePointsCsv(std::string_view text, const ImportOptions& options = {});
[[nodiscard]] Result<std::vector<Vec3>> parsePointsJson(std::string_view text, const ImportOptions& options = {});

/// Wählt das Format nach der Endung (".csv", ".txt", ".json", Groß-/Kleinschreibung egal).
/// Fehler: InvalidArgument bei unbekannter Endung.
[[nodiscard]] Result<std::vector<Vec3>> parsePoints(std::string_view text, std::string_view extension,
                                                    const ImportOptions& options = {});

}  // namespace whitehole::mapping
