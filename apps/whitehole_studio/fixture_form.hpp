// fixture_form.hpp: Formular für ein Fixture (Name, Lage, Drehung, Bauform, Form, Verkabelung).
//
// Zeigt Name, Lage, Drehung (3 Achsen) und Bauform; bei eigener Form (Matrix, Strip, 3D-Gitter) auch deren Felder; Punktwolken lassen sich nur
// in der Datei ändern. Jede Änderung geht über den Player, der danach den Ausgabeplan prüft. Wird
// sie abgelehnt (Universe belegt, Maß ungültig), meldet `failed` den Grund und das Formular springt
// auf den gespeicherten Wert zurück.
#pragma once

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "form_helpers.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class FixtureForm : public QWidget {
    Q_OBJECT

public:
    explicit FixtureForm(runtime::Player& player, QWidget* parent = nullptr);

    /// Zeigt das Fixture (oder, wenn es nicht (mehr) existiert, ein leeres Formular).
    void showFixture(mapping::FixtureId id);
    [[nodiscard]] mapping::FixtureId current() const noexcept { return id_; }

    /// Rauchtest für die CI: ändert Ursprung und Größe des angezeigten Fixtures über die
    /// Felder. Leer bei Erfolg, sonst der gescheiterte Schritt.
    QString exercise();
    /// Rauchtest für ein Fixture aus einer Bauform: dreht um die Achsen y und z und löst es von der Bauform. Leer bei Erfolg.
    QString exerciseTyped();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
    struct Controls {
        QLineEdit* name{nullptr};
        form::Vec3Boxes origin;
        form::Vec3Boxes rotation;  // Drehung in Grad um x, y, z
        QComboBox* type{nullptr};  // Bauform aus der Bibliothek; erster Eintrag = eigene Form
        // Matrix
        QSpinBox* columns{nullptr};
        QSpinBox* rows{nullptr};
        form::Vec2Boxes matrixPitch;
        QComboBox* corner{nullptr};
        QComboBox* firstAxis{nullptr};
        QCheckBox* matrixSerpentine{nullptr};
        // Strip
        QSpinBox* count{nullptr};
        form::Vec3Boxes start;
        form::Vec3Boxes end;
        // Gitter
        QSpinBox* nx{nullptr};
        QSpinBox* ny{nullptr};
        QSpinBox* nz{nullptr};
        form::Vec3Boxes gridPitch;
        QComboBox* axisOrder{nullptr};
        QCheckBox* gridSerpentine{nullptr};
        QCheckBox* flipX{nullptr};  // Startecke des Kabels: am oberen Ende der Achse beginnen
        QCheckBox* flipY{nullptr};
        QCheckBox* flipZ{nullptr};
    };

    void build();
    void buildShapeFields(QFormLayout& layout, const mapping::FixtureShape& shape);
    void report(const Result<void>& result, bool rebuildAfter = false);

    void applyName();
    void applyOrigin();
    void applyRotation();
    void applyType();
    void applyShape();

    [[nodiscard]] mapping::FixtureShape readShape(const mapping::FixtureShape& current) const;

    runtime::Player& player_;
    mapping::FixtureId id_;
    QVBoxLayout* outer_{nullptr};
    QWidget* content_{nullptr};
    Controls c_;
};

}  // namespace whitehole::studio
