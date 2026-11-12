// patch_table.hpp: Tabelle aller Fixtures mit ihrem Patch (Protokoll, Ziel, Universe, Kanal, Farbreihenfolge ...).
//
// Das Protokoll steht nicht hier, sondern beim Controller: die Spalte "Controller" wählt ihn, die Spalte Protokoll zeigt
// nur, was er spricht. Universe und Erster Kanal gelten bei Art-Net und sACN (Art-Net zählt ab 0, sACN ab 1), der DDP-Offset
// nur bei DDP; die jeweils anderen Zellen sind grau.
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
    // Protocol ist nur eine Anzeige: das Protokoll gehört zum Controller (Spalte Destination), nicht zum Fixture.
    enum Column { Name, Type, Leds, Send, Destination, Protocol, Universe, Channel, Range, DdpOffset, SkipFirst, SkipEvery, SkipCount, Order, Gamma, Brightness, Count };

    void commit(mapping::FixtureId id, bool rebuildAfter = false);
    void toggle(mapping::FixtureId id, bool on);
    void report(const Result<void>& result, bool rebuildAfter);
    [[nodiscard]] int rowOf(mapping::FixtureId id) const;
    [[nodiscard]] mapping::FixtureId idAt(int row) const;
    void updateEnabled(int row);
    /// Schreibt den belegten Adressraum der Zeile ("U1: 1–297") aus dem gespeicherten Patch in die Spalte Adressraum.
    void updateRange(int row);
    /// Das Protokoll, das die Zeile gerade anzeigt (kommt vom Controller; im Item der Spalte Protocol gemerkt).
    [[nodiscard]] output::Protocol protocolAt(int row) const;
    void saveHeader() const;

    runtime::Player& player_;
    bool populating_{false};
};

}  // namespace whitehole::studio
