// layout_panel.hpp: Seite "Layout" im Stage Config: Fixtures auf der Bühne verteilen, drehen und ausrichten.
//
// Liste der Fixtures (Hinzufügen per Dialog oder aus einer Bauform, Entfernen, Ausrichten) und das Formular zum
// gewählten Fixture (Lage, Drehung um x y z, Bauform). Patch und Ziele stehen auf der Seite Ausgabe (OutputPage), die große
// Bühnenansicht daneben im Fenster. Die Liste erlaubt mehrere Fixtures; dieselbe
// Auswahl zeigt die Vorschau (PreviewController). Das Panel hält keinen eigenen Zustand; alles geht
// über den Player, der jede Änderung samt Ausgabeplan prüft.
//
// Die Bauform-Bearbeitung (Anlegen, Maße, Verkabelung, Vorschau, Benutzerbibliothek) stand früher auf einer
// eigenen Seite "Fixtures" im Stage-Config-Fenster (FixtureLibraryPage); auf Wunsch des Nutzers zeigt diese Seite
// jetzt echte Fixture-Exemplare statt Bauformen (siehe fixtures_page.hpp), die Bauform-Bearbeitung ist hierher in
// den Reiter Layout gezogen, zur Fixture-Bibliothek (layout_panel_shapes.cpp, layout_panel_catalog.cpp).
#pragma once

#include <QListWidget>
#include <QString>
#include <QTabWidget>

#include <vector>

#include "add_fixture_dialog.hpp"
#include "fixture_form.hpp"
#include "shape_form.hpp"
#include "shape_preview.hpp"
#include "stage_view.hpp"
#include "whitehole/mapping/point_import.hpp"
#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;

namespace whitehole::studio {

class LayoutPanel : public QTabWidget {
    Q_OBJECT

public:
    explicit LayoutPanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Raster im Layout-Editor: anzeigen, Einrasten beim Ziehen, Weite in Layout-Einheiten.
    [[nodiscard]] bool gridVisible() const;
    [[nodiscard]] bool snapEnabled() const;
    /// Zusätzlich an Kanten und Mitten anderer Fixtures einrasten (nur wirksam, wenn Einrasten an ist).
    [[nodiscard]] bool fixtureSnapEnabled() const;
    void setFixtureSnap(bool on);
    [[nodiscard]] float gridStep() const;
    /// Setzt alle drei (für den Rauchtest); meldet `gridChanged`.
    void setGrid(bool show, bool snap, float step);

    /// Liest alles neu aus dem Player (nach Laden, Undo, Redo). Behält das gewählte Fixture, wenn es
    /// noch existiert.
    void rebuild();

    void addMatrix();
    void addStrip();
    /// Öffnet den Dialog "Fixture hinzufügen" (Form, Größe x y z, Lage, Verkabelung, Patch) und legt das Ergebnis an.
    void addFromDialog();
    /// Legt ein Fixture so an, wie der Dialog es beschreibt. Ohne Projekt entsteht zuerst ein neues.
    void addFixture(const NewFixture& fixture);
    /// Wie importPoints, aber mit gegebener Datei und ohne Dialog. Wahr, wenn ein Fixture entstanden ist.
    bool importPointsFile(const QString& path, const mapping::ImportOptions& options);
    void removeSelected();

    /// Gruppen und Regionen als Ziele für Output-Nodes: Menü füllen, Gruppe aus der Auswahl speichern, Region anlegen.
    void fillGroupMenu(QMenu* menu);
    void saveGroup();
    void addRegion();

    /// Die Arten, aus denen "Neu" eine Bauform macht (Reihenfolge wie im Menü) - wie früher FixtureLibraryPage::Kind.
    enum class ShapeKind { Matrix2d, Matrix3d, Circle, Strip, Column, Cylinder };  // Column = Spirale, Cylinder = Säule

    /// Bauformen dieser Show (project::FixtureType, nicht Fixture-Exemplare): anlegen, Maße ändern, umbenennen,
    /// vervielfältigen, entfernen. Dieselben Bauformen wie auf der Stage-Config-Seite Fixtures - nur dass sie
    /// jetzt hier bearbeitet werden (siehe Kommentar am Dateianfang).
    bool addShapeType(ShapeKind kind);
    [[nodiscard]] QString selectedShapeName() const;
    void selectShapeType(const QString& name);
    void removeSelectedShape();
    void duplicateSelectedShape();
    void renameSelectedShape(const QString& newName);

    /// Fixture-Bibliothek (alle gespeicherten Bauformen, Benutzerbibliothek auf der Festplatte, siehe
    /// user_library.hpp): Liste füllen und aus einer gewählten Bauform ein Exemplar mit Patch auf der Bühne
    /// anlegen (fehlt die Bauform in dieser Show noch, wird sie zuerst angelegt) - übernimmt damit auch die
    /// frühere Aufgabe der entfernten Liste "Fixtures in Show".
    void fillCatalogList(const QString& select = {});
    bool takeOverFromCatalog(const QString& name);
    [[nodiscard]] QString selectedCatalogName() const;
    /// Speichert die gewählte Bauform dieser Show in der Benutzerbibliothek (ersetzt eine gleichnamige).
    bool saveSelectedShapeToLibrary();
    /// Holt Bauformen der Benutzerbibliothek, die dem Projekt fehlen (gleichnamige bleiben unberührt; legt keine
    /// Exemplare an). Gibt die Zahl der neuen zurück.
    int importFromUserLibrary();
    /// Nimmt die gewählte gespeicherte Bauform aus der Benutzerbibliothek (die Show bleibt unberührt).
    void removeSelectedFromCatalog();

    /// Alle in der Liste gewählten Fixtures (in Listenreihenfolge).
    [[nodiscard]] std::vector<mapping::FixtureId> selectedIds() const;
    /// Wählt genau diese Fixtures (ohne `selectionChanged` auszulösen); das erste zeigt das Formular.
    void setSelectedIds(const std::vector<mapping::FixtureId>& ids);
    /// Liest das Fixture-Formular neu (nach Ziehen in der Vorschau), ohne die Auswahl zu ändern.
    void refreshValues();
    /// Bleibt für die Aufrufer erhalten (die Bühnenansicht liegt im Fenster und wird vom Hauptfenster gezeichnet).
    void refreshView();
    /// Richtet die gewählten Fixtures aus bzw. reiht sie aneinander (Abstand aus `packGap_`).
    void align(runtime::Player::Align mode);
    void pack(runtime::Player::Axis axis);

    /// Rauchtest für die CI (siehe FixtureForm::exercise und OutputForm::exercise). Leer bei Erfolg.
    QString exercise();

    [[nodiscard]] QSize sizeHint() const override { return {360, 520}; }

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Erfolgsmeldung ohne Fehler (z. B. „In Bibliothek speichern“); die Statusleiste zeigt sie kurz an.
    void notice(const QString& message);
    /// Es ist noch kein Projekt geladen, der Nutzer will aber ein Fixture anlegen: das Hauptfenster
    /// soll ein neues Projekt anlegen (direkt verbundenes Signal, danach ist `player_.project()` gesetzt).
    void projectNeeded();
    /// Der Nutzer hat in der Liste eine andere Auswahl getroffen.
    void selectionChanged();
    /// Raster gezeigt/ausgeblendet, Einrasten oder Rasterweite geändert.
    void gridChanged();

private:
    QWidget* buildArrangeBar();
    QWidget* buildGridBar();
    /// Baut den Bereich "Bauformen dieser Show" (layout_panel_shapes.cpp): Liste, Name, Formular, Vorschau,
    /// Knöpfe Neu/Duplizieren/Entfernen/In Bibliothek speichern.
    QWidget* buildShapesArea();
    QString exerciseArrange();
    QString exerciseImport();
    QString exerciseAddDialog();
    QString exerciseCatalog();
    /// Rauchtest der Bauform-Bearbeitung (früher FixtureLibraryPage::exercise, siehe layout_panel_shapes.cpp).
    QString exerciseShapes();
    void fillList(mapping::FixtureId select);
    void refreshTexts();
    void addFixture(const QString& baseName, mapping::FixtureShape shape);
    [[nodiscard]] QString uniqueName(const QString& base) const;
    [[nodiscard]] Vec3 nextOrigin() const;
    [[nodiscard]] mapping::FixtureId selectedId() const;
    /// Rechtsklick auf die Fixture-Liste: entfernt das Fixture unter dem Mauszeiger, unabhängig von der
    /// aktuellen Auswahl (auf leerer Fläche ohne Wirkung).
    void showListContextMenu(const QPoint& pos);
    void removeFixture(mapping::FixtureId id);

    /// Bauform-Bearbeitung (layout_panel_shapes.cpp): Liste füllen, Formular/Vorschau nach Auswahl zeigen,
    /// eindeutigen Namen vorschlagen, einen Result<void> melden (edited oder failed+zurücksetzen).
    void fillShapeList(const QString& select);
    void showSelectedShape();
    [[nodiscard]] QString uniqueShapeName(const QString& base) const;
    void reportShape(const Result<void>& result);
    void applyShapeEdit(const mapping::FixtureShape& shape);

    /// Fixture-Bibliothek (layout_panel_catalog.cpp): wie fillShapeList, nur für die gespeicherte Liste;
    /// beide Listen schließen sich gegenseitig in der Auswahl aus (siehe layout_panel_catalog.cpp).
    void showSelectedCatalog();

    runtime::Player& player_;
    QListWidget* list_{nullptr};
    QListWidget* catalog_{nullptr};  // Fixture-Bibliothek (alle gespeicherten Bauformen); Doppelklick legt ein Exemplar an
    QPushButton* catalogTakeOver_{nullptr};
    QPushButton* catalogImport_{nullptr};
    QPushButton* catalogSave_{nullptr};
    QPushButton* catalogDelete_{nullptr};
    FixtureForm* fixture_{nullptr};
    QCheckBox* showGrid_{nullptr};
    QCheckBox* snap_{nullptr};
    QCheckBox* snapFixtures_{nullptr};
    QComboBox* gridStep_{nullptr};
    QDoubleSpinBox* packGap_{nullptr};  // Abstand für "Nebeneinander"/"Übereinander"

    // Bauformen dieser Show (project::FixtureType) - früher FixtureLibraryPage, siehe Kommentar am Dateianfang.
    QListWidget* shapeList_{nullptr};
    QLineEdit* shapeName_{nullptr};
    QLabel* shapeUsage_{nullptr};
    ShapeForm* shapeForm_{nullptr};
    ShapePreview* shapePreview_{nullptr};
    QPushButton* shapeAddButton_{nullptr};
    QPushButton* shapeDuplicate_{nullptr};
    QPushButton* shapeRemove_{nullptr};
    bool shapeEditing_{false};  // wir ändern gerade selbst: showSelectedShape() fasst die Felder nicht an
};

}  // namespace whitehole::studio
