// output_page.hpp: Seite "Ausgabe" im Stage-Config-Fenster: Ziele, Einstellungen und der Patch aller Fixtures.
//
// Warum es diese Datei gibt: Wohin die Daten gehen (Controller-Adressen), welches Fixture auf welches Universe sendet und
// wohin der interne DMX-Kanal-Pool der Output-Nodes gepatcht ist, gehört zusammen und wird beim Einrichten der Ausgabe
// gebraucht, nicht beim Anordnen der Fixtures. Die Seite fasst das Ausgabeformular (Ziele, Bildrate ...), die
// Fixture-Patch-Tabelle (ein Fixture je Zeile) und die DMX-Kanal-Pool-Tabelle (dmx_patch_table.hpp) zusammen.
#pragma once

#include <QString>
#include <QWidget>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class OutputForm;
class PatchTable;
class DmxPatchTable;

class OutputPage : public QWidget {
    Q_OBJECT

public:
    explicit OutputPage(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest alles neu aus dem Player (nach Laden, Undo, Redo). Während eigener Änderungen passiert nichts.
    void rebuild();
    /// Wie rebuild(), aber nur bei sichtbarer Seite; sonst beim nächsten Anzeigen.
    void refresh();

    /// Rauchtest für die CI (Ziele und Patch-Tabelle). Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Hinweis ohne Fehler (zum Beispiel verschobene Universe beim Protokollwechsel) für die Statuszeile.
    void notice(const QString& message);

protected:
    void showEvent(QShowEvent* event) override;

private:
    OutputForm* form_;
    PatchTable* patches_;
    DmxPatchTable* dmxPatches_;
    bool dirty_{false};
    bool editing_{false};  // Änderung kommt von dieser Seite: die Zellen nicht unter dem Cursor austauschen
};

}  // namespace whitehole::studio
