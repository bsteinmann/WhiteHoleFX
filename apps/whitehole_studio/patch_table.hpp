// patch_table.hpp: Tabelle aller Fixtures mit ihrem Patch (Protokoll, Ziel, Universe, Kanal, Farbreihenfolge ...).
//
// Warum es diese Datei gibt: Beim Einrichten der Ausgabe will man alle Fixtures auf einen Blick sehen und patchen:
// welches Fixture sendet wohin, wo überlappen sich Universen. Früher stand der Patch im Formular des einzelnen Fixtures;
// jetzt gehört er zur Ausgabe. Jede Zeile ist ein Fixture, jede Zelle ein Feld; eine Änderung geht über den Player,
// der den Ausgabeplan prüft. Wird sie abgelehnt (Universe belegt ...), meldet `failed` den Grund und die Zeile springt
// auf den gespeicherten Wert zurück. Die Spalten lassen sich verschieben; die Reihenfolge wird gemerkt.
#pragma once

#include <QString>
#include <QTableWidget>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class PatchTable : public QTableWidget {
    Q_OBJECT

public:
    explicit PatchTable(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest alle Zeilen neu aus dem Player (nach Laden, Undo, Redo, Änderungen am Layout).
    void rebuild();

    /// Rauchtest für die CI: Farbreihenfolge ändern, Senden aus- und einschalten, Spalte verschieben. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
    enum Column { Name, Type, Leds, Send, Protocol, Destination, Universe, Channel, DdpOffset, Order, Gamma, Brightness, Count };

    void commit(mapping::FixtureId id);
    void toggle(mapping::FixtureId id, bool on);
    void report(const Result<void>& result, bool rebuildAfter);
    [[nodiscard]] int rowOf(mapping::FixtureId id) const;
    [[nodiscard]] mapping::FixtureId idAt(int row) const;
    void updateEnabled(int row);
    void saveHeader() const;

    runtime::Player& player_;
    bool populating_{false};
};

}  // namespace whitehole::studio
