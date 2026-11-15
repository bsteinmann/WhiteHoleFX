// dmx_cell_colors.hpp: Standardfarben für die Zellen der beiden DMX-Monitore nach Belegungsart.
//
// Warum es das gibt: erster Teil des 15.11.-Vorschlags "farbig hinterlegen" (siehe CLAUDE.md,
// DMX-Patch/Monitor-Gitter), nachträglich freigegeben - je eine Farbe für Fixture, DMX-Kanal-Pool, Master,
// Live Kanal und einen vorhandenen Node ("DMX Out"), damit man auf einen Blick sieht, wie ein Kanal benutzt
// wird, ohne jede Zelle einzeln anzuhovern. Eigene, kleine Datei statt die Farblogik in den beiden Panels
// zu verdoppeln - dmx_monitor_panel.cpp (Eingang) und dmx_output_monitor_panel.cpp (Ausgang) greifen beide
// darauf zu. Reine Zuordnungstabelle, kein Zustand; Rechtsklick-Überschreibung einzelner Zellen bleibt in
// den Panels selbst (dort liegt auch, welche Zelle gerade angezeigt wird).
#pragma once

#include <QColor>

#include "whitehole/runtime/dmx_listener.hpp"
#include "whitehole/runtime/dmx_output_usage.hpp"

namespace whitehole::studio {

enum class DmxCellCategory { None, Fixture, Pool, Master, Live, Node };

/// Standardfarbe einer Kategorie; None liefert eine ungültige QColor (QColor::isValid() == false) - der
/// Aufrufer setzt dann den normalen Tabellenhintergrund zurück, statt eine Farbe zu malen.
[[nodiscard]] QColor dmxCellColor(DmxCellCategory category);

/// Kategorie eines Eingangs-Hörers: Master/Live Kanal bleiben eigene Kategorien, ein Graph-Node (auch über
/// "DMX Out" gepatcht) zählt als Node.
[[nodiscard]] DmxCellCategory dmxCellCategory(const runtime::DmxInputListener& listener);

/// Kategorie einer Ausgangs-Belegung: Fixture-Patch oder DMX-Kanal-Pool.
[[nodiscard]] DmxCellCategory dmxCellCategory(const runtime::DmxOutputOccupant& occupant);

}  // namespace whitehole::studio
