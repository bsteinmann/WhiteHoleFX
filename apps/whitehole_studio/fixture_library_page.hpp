// fixture_library_page.hpp: Seite "Fixtures" im Stage-Config-Fenster: die Bibliothek der Bauformen.
//
// Warum es diese Datei gibt: Eine Bauform (zum Beispiel "Matrix 8x8" oder "Säule 60") wird einmal beschrieben und
// dann im Reiter Layout beliebig oft auf der Bühne verteilt. Hier legt man Bauformen an (2D-Matrix, 3D-Matrix, Kreis,
// Strip, Säule), stellt ihre Maße und Verkabelung ein und sieht rechts, wie sie aussehen; der Knopf "Test" lässt ein
// Muster darüber laufen. Ändert man eine Bauform, folgen alle Exemplare im Layout (Player::replaceFixtureType).
//
// Die Seite hält keinen Zustand; alles liegt im Projekt und geht über den Player (Rückgängig inklusive). Zusätzlich
// wird jede Änderung in die Benutzerbibliothek (user_library.hpp) gespiegelt, damit angelegte Bauformen auch neue
// Projekte und neue Programmversionen überleben.
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
    /// Die Arten, aus denen "Neu" eine Bauform macht (Reihenfolge wie im Menü).
    enum class Kind { Matrix2d, Matrix3d, Circle, Strip, Column };

    explicit FixtureLibraryPage(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest die Bibliothek neu aus dem Player (nach Laden, Undo, Redo). Behält die gewählte Bauform, wenn es sie noch gibt.
    void rebuild();
    /// Wie rebuild(), aber nur wenn die Seite sichtbar ist; sonst beim nächsten Anzeigen (Zeichnen, solange niemand hinsieht, wäre vergeudet).
    void refresh();
    /// Legt eine Bauform der Art mit Standardmaßen an und wählt sie. Wahr bei Erfolg.
    bool addType(Kind kind);
    /// Name der gewählten Bauform, leer ohne Auswahl.
    [[nodiscard]] QString selectedName() const;
    void selectType(const QString& name);
    void removeSelected();
    void duplicateSelected();
    void renameSelected(const QString& newName);
    /// Holt Bauformen der Benutzerbibliothek, die dem Projekt fehlen (gleichnamige bleiben unberührt). Gibt die Zahl der neuen zurück.
    int importFromUserLibrary();

    /// Rauchtest für die CI: jede Art anlegen, Maße ändern, umbenennen, vervielfältigen, entfernen. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Es ist noch kein Projekt geladen: das Hauptfenster soll eines anlegen (direkt verbunden, danach gibt es eins).
    void projectNeeded();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void fillList(const QString& select);
    void showSelected();
    void applyShape(const mapping::FixtureShape& shape);
    void report(const Result<void>& result);
    /// Spiegelt die Bauform (oder ihr Entfernen/Umbenennen) in die Benutzerbibliothek auf der Festplatte.
    void mirrorToUserLibrary(const QString& name);
    void mirrorRemoval(const QString& name);
    void mirrorRename(const QString& from, const QString& to);
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
    QPushButton* import_;
    bool dirty_{false};    // Projekt hat sich geändert, während die Seite unsichtbar war
    bool editing_{false};  // wir ändern gerade selbst: rebuild() fasst die Felder nicht an
};

}  // namespace whitehole::studio
