// fixture_library_page.hpp: Seite "Fixtures" im Stage-Config-Fenster: Fixtures anlegen, bearbeiten, mit Vorschau.
//
// Warum es diese Datei gibt: Ein Fixture (zum Beispiel "LED-Wand 12x24" oder "Würfel 8x8x8") wird einmal
// beschrieben - Geometrie, Maße, Verkabelung - und im Reiter Layout dann beliebig oft (mit Anzahl) auf der Bühne
// gepatcht. Hier legt man Fixtures an (2D-Matrix, 3D-Matrix, Kreis, Strip, Säule, Spirale), stellt ihre Maße und
// Verkabelung ein und sieht rechts, wie sie aussehen; der Knopf "Test" lässt ein Muster darüber laufen. Ändert man
// ein Fixture, folgen alle Exemplare im Layout. Adressiert (DMX-Patch) wird hier nichts - das passiert erst beim
// Platzieren eines Exemplars im Reiter Layout.
//
// Genau eine Liste: die Fixtures dieser Show (vom Nutzer gewünscht - keine automatisch geladene, im Hintergrund
// zusammengeführte Bibliothek mehr). Ein Fixture für ein anderes Projekt wiederverwenden läuft über **Export**
// (als eigene Datei irgendwohin speichern, project::writeFixtureExport) und **Import** (eine solche Datei laden,
// project::parseFixtureExport) - der Nutzer wählt den Pfad jedes Mal selbst per Dateidialog, nichts passiert
// automatisch im Hintergrund.
#pragma once

#include <QString>
#include <QWidget>

#include "whitehole/mapping/fixture.hpp"
#include "whitehole/runtime/player.hpp"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace whitehole::studio {

class ShapeForm;
class ShapePreview;

class FixtureLibraryPage : public QWidget {
    Q_OBJECT

public:
    /// Die Arten, aus denen "Neu" ein Fixture macht (Reihenfolge wie im Menü).
    enum class Kind { Matrix2d, Matrix3d, Circle, Strip, Column, Cylinder };  // Column = Spirale, Cylinder = Säule

    explicit FixtureLibraryPage(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest die Liste neu aus dem Projekt (nach Laden, Undo, Redo). Behält das gewählte Fixture, wenn es noch da
    /// ist.
    void rebuild();
    /// Wie rebuild(), aber nur wenn die Seite sichtbar ist; sonst beim nächsten Anzeigen.
    void refresh();
    /// Legt ein neues Fixture der Art mit Standardmaßen in dieser Show an und wählt es. Wahr bei Erfolg.
    bool addType(Kind kind);
    /// Name des gewählten Fixtures, leer ohne Auswahl.
    [[nodiscard]] QString selectedName() const;
    void selectType(const QString& name);
    /// Entfernt das gewählte Fixture aus dieser Show.
    void removeSelected();
    /// Kopiert das gewählte Fixture unter neuem Namen.
    void duplicateSelected();
    void renameSelected(const QString& newName);
    /// Schreibt das gewählte Fixture nach `path` (project::writeFixtureExport). Wahr bei Erfolg.
    bool exportSelectedTo(const QString& path);
    /// Liest Fixtures aus `path` (project::parseFixtureExport) und legt sie in dieser Show an - gleichnamige
    /// werden mit einer Zahl eindeutig gemacht, keine wird überschrieben. Zahl der angelegten Fixtures, oder -1
    /// bei einem Lesefehler (dann über `failed`).
    int importFrom(const QString& path);

    /// Rauchtest für die CI: jede Art anlegen, Maße ändern, umbenennen, vervielfältigen, exportieren, entfernen,
    /// wieder importieren. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Kurze Rückmeldung (Statuszeile), zum Beispiel „exportiert“.
    void notice(const QString& message);
    /// Es ist noch kein Projekt geladen: das Hauptfenster soll eines anlegen (direkt verbunden, danach gibt es eins).
    void projectNeeded();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void fillList(const QString& select);
    void showSelected();
    void applyShape(const mapping::FixtureShape& shape);
    void report(const Result<void>& result);
    [[nodiscard]] QString uniqueName(const QString& base) const;

    runtime::Player& player_;
    QListWidget* list_;
    QLineEdit* name_;
    QLabel* usage_;
    ShapeForm* form_;
    ShapePreview* preview_;
    QPushButton* addButton_;
    QPushButton* duplicate_;
    QPushButton* remove_;
    QPushButton* export_;
    QPushButton* import_;
    bool dirty_{false};    // Projekt hat sich geändert, während die Seite unsichtbar war
    bool editing_{false};  // wir ändern gerade selbst: rebuild() fasst die Felder nicht an
};

}  // namespace whitehole::studio
