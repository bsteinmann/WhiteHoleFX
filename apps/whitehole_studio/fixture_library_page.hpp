// fixture_library_page.hpp: Seite "Fixtures" im Stage-Config-Fenster: Fixtures anlegen, bearbeiten, mit Vorschau.
//
// Warum es diese Datei gibt: Ein Fixture (zum Beispiel "LED-Wand 12x24" oder "Würfel 8x8x8") wird einmal
// beschrieben - Geometrie, Maße, Verkabelung - und im Reiter Layout dann beliebig oft (mit Anzahl) auf der Bühne
// gepatcht. Hier legt man Fixtures an (2D-Matrix, 3D-Matrix, Kreis, Strip, Säule, Spirale), stellt ihre Maße und
// Verkabelung ein und sieht rechts, wie sie aussehen; der Knopf "Test" lässt ein Muster darüber laufen. Ändert man
// ein Fixture, folgen alle Exemplare im Layout (Player::replaceFixtureType). Adressiert (DMX-Patch) wird hier
// nichts - das passiert erst beim Platzieren eines Exemplars im Reiter Layout.
//
// Eine einzige Liste statt getrennter "Fixtures dieser Show" / "Gespeicherte Fixtures" (vom Nutzer gewünscht, nach
// dem Vorbild einer Lichtpult-Fixture-Bibliothek): sie zeigt beides zusammen (siehe fixture_catalog.hpp). Ein
// Fixture, das nur in der Bibliothek liegt, wird beim ersten Bearbeiten automatisch in diese Show übernommen
// (ensureInProject) - kein eigener "Übernehmen"-Schritt mehr nötig. "Entfernen" wirkt auf die Show, "Aus
// Bibliothek löschen" auf die Datei auf der Festplatte (user_library.hpp); beides unabhängig voneinander, je
// nachdem, wo das gewählte Fixture tatsächlich existiert.
//
// Die Seite hält sonst keinen Zustand; alles liegt im Projekt und geht über den Player (Rückgängig inklusive).
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

    /// Liest die Liste neu aus Projekt und Bibliothek (nach Laden, Undo, Redo). Behält das gewählte Fixture, wenn
    /// es noch da ist.
    void rebuild();
    /// Wie rebuild(), aber nur wenn die Seite sichtbar ist; sonst beim nächsten Anzeigen.
    void refresh();
    /// Legt ein neues Fixture der Art mit Standardmaßen in dieser Show an und wählt es. Wahr bei Erfolg.
    bool addType(Kind kind);
    /// Name des gewählten Fixtures, leer ohne Auswahl.
    [[nodiscard]] QString selectedName() const;
    void selectType(const QString& name);
    /// Entfernt das gewählte Fixture aus dieser Show (nur wirksam, wenn es dort existiert; eine nur in der
    /// Bibliothek gespeicherte Form bleibt unberührt - dafür gibt es removeFromLibrary()).
    void removeSelected();
    /// Kopiert das gewählte Fixture (gleich, ob aus der Show oder nur aus der Bibliothek) unter neuem Namen in
    /// diese Show.
    void duplicateSelected();
    /// Benennt das gewählte Fixture um; übernimmt es dafür zuerst in diese Show, falls es nur in der Bibliothek lag.
    void renameSelected(const QString& newName);
    /// Speichert das gewählte Fixture dieser Show in der Benutzerbibliothek (ersetzt ein gleichnamiges).
    bool saveSelectedToLibrary();
    /// Nimmt das gewählte Fixture aus der Benutzerbibliothek (nur wirksam, wenn es dort liegt; die Show bleibt
    /// unberührt).
    void removeFromLibrary();

    /// Rauchtest für die CI: jede Art anlegen, Maße ändern, umbenennen, vervielfältigen, in die Bibliothek
    /// speichern, aus der Show entfernen und über eine Bearbeitung automatisch zurückholen, entfernen. Leer bei
    /// Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Kurze Rückmeldung (Statuszeile), zum Beispiel „gespeichert“.
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
    /// Ist `name` schon ein Fixture dieser Show? Wenn nicht, aber in der Bibliothek gespeichert, wird es jetzt
    /// angelegt (dieselbe Form). Falsch, wenn weder das eine noch das andere gilt, oder ohne Projekt keines
    /// angelegt werden konnte.
    bool ensureInProject(const QString& name);

    runtime::Player& player_;
    QListWidget* list_;
    QLineEdit* name_;
    QLabel* usage_;
    ShapeForm* form_;
    ShapePreview* preview_;
    QPushButton* addButton_;
    QPushButton* duplicate_;
    QPushButton* remove_;
    QPushButton* saveToLibrary_;
    QPushButton* deleteFromLibrary_;
    bool dirty_{false};    // Projekt hat sich geändert, während die Seite unsichtbar war
    bool editing_{false};  // wir ändern gerade selbst: rebuild() fasst die Felder nicht an
};

}  // namespace whitehole::studio
