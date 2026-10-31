// region_form.hpp: Formular für den Effektbereich (Bild-in-Bild).
//
// Warum es diese Datei gibt: Soll ein Effekt nur einen Teil der Wand füllen, gibt man hier das
// Rechteck an (in Prozent der Wand, x und y von links oben). Dasselbe Rechteck lässt sich in der
// Vorschau zeichnen und ziehen: der Knopf "In der Vorschau zeichnen" schaltet diesen Modus ein.
// Wie die anderen Formulare hält es keinen eigenen Zustand; der Player prüft jede Änderung.
#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class RegionForm : public QWidget {
    Q_OBJECT

public:
    explicit RegionForm(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest den Bereich neu aus dem Player (nach Laden, Undo, Redo und Ziehen in der Vorschau).
    void rebuild();

    [[nodiscard]] bool drawMode() const { return draw_->isChecked(); }
    void setDrawMode(bool on);

    /// Rauchtest für die CI: Bereich einschalten, Breite ändern, wieder ausschalten. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    void drawModeChanged(bool on);

private:
    void applyEnabled(bool on);
    void applyValues();

    runtime::Player& player_;
    QCheckBox* enabled_{nullptr};
    QDoubleSpinBox* x_{nullptr};
    QDoubleSpinBox* y_{nullptr};
    QDoubleSpinBox* w_{nullptr};
    QDoubleSpinBox* h_{nullptr};
    QPushButton* draw_{nullptr};
    bool populating_{false};
};

}  // namespace whitehole::studio
