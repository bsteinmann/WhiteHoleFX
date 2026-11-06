// shape_form.hpp: Formular für eine Fixture-Form (Matrix, 3D-Gitter, Ring, Strip, Säule).
//
// Warum es diese Datei gibt: Im neuen Stage-Config-Fenster wird die Bauform eines Fixtures in der Bibliothek
// beschrieben, ohne dass schon ein Fixture im Raum steht. Das alte FixtureForm war an ein Fixture mit Nummer
// gebunden; dieses Formular bearbeitet nur den Wert einer Form und meldet jede Änderung mit `shapeEdited`.
// Was daraus wird (Bibliothek, Prüfung, Rückgängig), entscheidet der Aufrufer. Lehnt er die Form ab, setzt er
// mit setShape() den gespeicherten Wert zurück.
//
// Felder melden erst bei Enter, Pfeiltasten oder Fokuswechsel (siehe form_helpers.hpp).
#pragma once

#include <QWidget>

#include "form_helpers.hpp"
#include "whitehole/mapping/fixture.hpp"

class QCheckBox;
class QFormLayout;
class QVBoxLayout;

namespace whitehole::studio {

class ShapeForm : public QWidget {
    Q_OBJECT

public:
    explicit ShapeForm(QWidget* parent = nullptr);

    /// Baut die Felder für diese Form neu auf (meldet nichts).
    void setShape(const mapping::FixtureShape& shape);
    [[nodiscard]] const mapping::FixtureShape& shape() const noexcept { return shape_; }

    /// Rauchtest für die CI: ändert über die Felder die Größe und prüft, dass die Änderung gemeldet wird. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void shapeEdited(const whitehole::mapping::FixtureShape& shape);

private:
    struct Controls {
        // Matrix
        QSpinBox* columns{nullptr};
        QSpinBox* rows{nullptr};
        form::Vec2Boxes matrixPitch;
        QComboBox* corner{nullptr};
        QComboBox* firstAxis{nullptr};
        QCheckBox* matrixSerpentine{nullptr};
        // Strip
        QSpinBox* count{nullptr};  // auch Ring und Säule
        form::Vec3Boxes start;
        form::Vec3Boxes end;
        // Gitter
        QSpinBox* nx{nullptr};
        QSpinBox* ny{nullptr};
        QSpinBox* nz{nullptr};
        form::Vec3Boxes gridPitch;
        QComboBox* axisOrder{nullptr};
        QCheckBox* gridSerpentine{nullptr};
        QCheckBox* flipX{nullptr};
        QCheckBox* flipY{nullptr};
        QCheckBox* flipZ{nullptr};
        // Ring und Säule
        QDoubleSpinBox* radius{nullptr};
        QDoubleSpinBox* height{nullptr};
        QDoubleSpinBox* turns{nullptr};
        QDoubleSpinBox* startAngle{nullptr};
        QCheckBox* clockwise{nullptr};
    };

    void build();
    void buildMatrix(QFormLayout& layout, const mapping::MatrixShape& m);
    void buildStrip(QFormLayout& layout, const mapping::StripShape& s);
    void buildGrid(QFormLayout& layout, const mapping::GridShape& g);
    void buildRing(QFormLayout& layout, const mapping::RingShape& r);
    void buildHelix(QFormLayout& layout, const mapping::HelixShape& h);
    void changed();
    [[nodiscard]] mapping::FixtureShape read() const;

    mapping::FixtureShape shape_{mapping::MatrixShape{}};
    QVBoxLayout* outer_{nullptr};
    QWidget* content_{nullptr};
    Controls c_;
};

}  // namespace whitehole::studio
