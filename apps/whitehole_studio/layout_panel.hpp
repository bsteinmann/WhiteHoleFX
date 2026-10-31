// layout_panel.hpp: Dock zum Bearbeiten von Fixtures, Patches, Zielen und Einstellungen.
//
// Zwei Seiten: "Fixtures" (Liste, Hinzufügen/Entfernen, Ausrichten, Formular zum gewählten
// Fixture) und "Ausgabe" (Ziele und Einstellungen). Die Liste erlaubt mehrere Fixtures; dieselbe
// Auswahl zeigt die Vorschau (PreviewController). Das Panel hält keinen eigenen Zustand; alles geht
// über den Player, der jede Änderung samt Ausgabeplan prüft.
#pragma once

#include <QListWidget>
#include <QString>
#include <QTabWidget>

#include <vector>

#include "fixture_form.hpp"
#include "output_form.hpp"
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
    /// Der Nutzer hat in der Liste eine andere Auswahl getroffen.
    void selectionChanged();

private:
    QWidget* buildArrangeBar();
    QString exerciseArrange();
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
