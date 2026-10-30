// fixture_form.hpp: Formular für ein Fixture (Name, Lage, Form, Verkabelung, Patch).
//
// Zeigt je nach Form (Matrix, Strip, 3D-Gitter) die passenden Felder; Punktwolken lassen sich nur
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

    /// Rauchtest für die CI: ändert Ursprung, Größe und Patch des angezeigten Fixtures über die
    /// Felder. Leer bei Erfolg, sonst der gescheiterte Schritt.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
    struct Controls {
        QLineEdit* name{nullptr};
        form::Vec3Boxes origin;
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
        // Patch
        QCheckBox* patched{nullptr};
        QComboBox* protocol{nullptr};
        QSpinBox* destination{nullptr};
        QSpinBox* universe{nullptr};
        QSpinBox* channel{nullptr};
        QSpinBox* ddpOffset{nullptr};
        QComboBox* colorOrder{nullptr};
        QDoubleSpinBox* gamma{nullptr};
        QDoubleSpinBox* brightness{nullptr};
    };

    void build();
    void buildShapeFields(QFormLayout& layout, const mapping::FixtureShape& shape);
    void buildPatchFields(QFormLayout& layout, const output::FixturePatch* patch);
    void updatePatchEnabled();
    void report(const Result<void>& result, bool rebuildAfter = false);

    void applyName();
    void applyOrigin();
    void applyShape();
    void applyPatch();
    void togglePatch(bool on);

    [[nodiscard]] mapping::FixtureShape readShape(const mapping::FixtureShape& current) const;
    [[nodiscard]] const output::FixturePatch* patchOf(mapping::FixtureId id) const;

    runtime::Player& player_;
    mapping::FixtureId id_;
    QVBoxLayout* outer_{nullptr};
    QWidget* content_{nullptr};
    Controls c_;
};

}  // namespace whitehole::studio
