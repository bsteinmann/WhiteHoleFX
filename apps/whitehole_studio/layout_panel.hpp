// layout_panel.hpp: Dock zum Bearbeiten von Fixtures, Patches, Zielen und Einstellungen.
//
// Zwei Seiten: "Fixtures" (Liste, Hinzufügen/Entfernen, Formular zum gewählten Fixture) und
// "Ausgabe" (Ziele und Einstellungen). Das Panel hält keinen eigenen Zustand; alles geht über den
// Player, der jede Änderung samt Ausgabeplan prüft.
#pragma once

#include <QListWidget>
#include <QString>
#include <QTabWidget>

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

    /// Rauchtest für die CI (siehe FixtureForm::exercise und OutputForm::exercise). Leer bei Erfolg.
    QString exercise();

    [[nodiscard]] QSize sizeHint() const override { return {360, 520}; }

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
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
