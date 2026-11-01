// layout_panel.hpp: Dock zum Bearbeiten von Fixtures, Patches, Zielen und Einstellungen.
//
// Zwei Seiten: "Fixtures" (Liste, Hinzufügen per Dialog/Entfernen, Ausrichten, Formular zum gewählten
// Fixture) und "Ausgabe" (Ziele und Einstellungen). Die Liste erlaubt mehrere Fixtures; dieselbe
// Auswahl zeigt die Vorschau (PreviewController). Das Panel hält keinen eigenen Zustand; alles geht
// über den Player, der jede Änderung samt Ausgabeplan prüft.
#pragma once

#include <QListWidget>
#include <QString>
#include <QTabWidget>

#include <vector>

#include "add_fixture_dialog.hpp"
#include "fixture_form.hpp"
#include "output_form.hpp"
#include "whitehole/mapping/point_import.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class LayoutPanel : public QTabWidget {
    Q_OBJECT

public:
    explicit LayoutPanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest alles neu aus dem Player (nach Laden, Undo, Redo). Behält das gewählte Fixture, wenn es
    /// noch existiert.
    void rebuild();

    void addMatrix();
    void addStrip();
    /// Öffnet den Dialog "Fixture hinzufügen" (Form, Größe x y z, Lage, Verkabelung, Patch) und legt das Ergebnis an.
    void addFromDialog();
    /// Legt ein Fixture so an, wie der Dialog es beschreibt. Ohne Projekt entsteht zuerst ein neues.
    void addFixture(const NewFixture& fixture);
    /// Fragt nach einer CSV-/JSON-Datei mit LED-Positionen und legt daraus ein Punktwolken-Fixture an.
    void importPoints();
    /// Wie importPoints, aber mit gegebener Datei und ohne Dialog. Wahr, wenn ein Fixture entstanden ist.
    bool importPointsFile(const QString& path, const mapping::ImportOptions& options);
    void removeSelected();

    /// Alle in der Liste gewählten Fixtures (in Listenreihenfolge).
    [[nodiscard]] std::vector<mapping::FixtureId> selectedIds() const;
    /// Wählt genau diese Fixtures (ohne `selectionChanged` auszulösen); das erste zeigt das Formular.
    void setSelectedIds(const std::vector<mapping::FixtureId>& ids);
    /// Liest das Fixture-Formular neu (nach Ziehen in der Vorschau), ohne die Auswahl zu ändern.
    void refreshValues();
    /// Richtet die gewählten Fixtures aus bzw. verteilt sie.
    void align(runtime::Player::Align mode);
    void distribute(runtime::Player::Axis axis);

    /// Rauchtest für die CI (siehe FixtureForm::exercise und OutputForm::exercise). Leer bei Erfolg.
    QString exercise();

    [[nodiscard]] QSize sizeHint() const override { return {360, 520}; }

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Es ist noch kein Projekt geladen, der Nutzer will aber ein Fixture anlegen: das Hauptfenster
    /// soll ein neues Projekt anlegen (direkt verbundenes Signal, danach ist `player_.project()` gesetzt).
    void projectNeeded();
    /// Der Nutzer hat in der Liste eine andere Auswahl getroffen.
    void selectionChanged();

private:
    QWidget* buildArrangeBar();
    QString exerciseArrange();
    QString exerciseImport();
    QString exerciseAddDialog();
    void fillList(mapping::FixtureId select);
    void refreshTexts();
    void addFixture(const QString& baseName, mapping::FixtureShape shape);
    [[nodiscard]] QString uniqueName(const QString& base) const;
    [[nodiscard]] Vec3 nextOrigin() const;
    [[nodiscard]] mapping::FixtureId selectedId() const;

    runtime::Player& player_;
    QListWidget* list_{nullptr};
    FixtureForm* fixture_{nullptr};
    OutputForm* output_{nullptr};
};

}  // namespace whitehole::studio
