// dmx_patch_table.hpp: Tabelle der dmxpatch-Einträge (interner DMX-Kanal-Pool der Output-Nodes auf echte
// Controller/Universe/Kanäle abgebildet, siehe docs/output.md).
//
// Anders als die Fixture-Patch-Tabelle (patch_table.hpp, eine Zeile je Fixture, immer vorhanden) ist eine
// Zeile hier kein fester Gegenstand: Es gibt kein "das eine" DmxChannelPatch zu jedem Pool-Kanal, der Nutzer
// legt beliebig viele, beliebig geschnittene Bereiche selbst an. Darum folgt dieses Formular dem Muster von
// OutputForm (Ziele-Tabelle): Zeilen per "+"/"-"-Button, jede Änderung baut die ganze Liste aus allen Zeilen
// neu und schickt sie als Ganzes an Player::setDmxPatches. Der Player prüft (OutputPipeline::create: Ziel
// muss existieren, kein DDP, Universe/Kanal/Pool-Kanal im gültigen Bereich, keine zwei Einträge mit
// überschneidendem Pool-Bereich); bei Ablehnung meldet `failed` den Grund und die Tabelle springt zurück.
#pragma once

#include <QString>
#include <QTableWidget>
#include <QWidget>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class DmxPatchTable : public QWidget {
    Q_OBJECT

public:
    explicit DmxPatchTable(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest alle Zeilen neu aus dem Player (nach Laden, Undo, Redo, Zielwechsel).
    void rebuild();

    /// Rauchtest für die CI: Patch anlegen, Werte ändern, einen zweiten anlegen, einen entfernen. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
    enum Column { PoolFrom, PoolCount, Destination, Protocol, Universe, Channel, Count };

    void addPatch();
    /// Entfernt den gewählten Patch (beliebig, nicht nur den letzten). Ohne Auswahl den letzten.
    void removeSelectedPatch();
    void applyPatches();
    void report(const Result<void>& result);

    runtime::Player& player_;
    QTableWidget* table_{nullptr};
    bool populating_{false};
};

}  // namespace whitehole::studio
