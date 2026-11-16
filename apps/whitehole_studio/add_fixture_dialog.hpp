// add_fixture_dialog.hpp: der Dialog "Fixture hinzufügen" (Form, Größe, Lage, Verkabelung, Patch).
//
// Warum es das gibt: Eine 3D-Pixel-Stage (Cube, Vorhang, Säule) aus festen Knöpfen anzulegen und danach
// Feld für Feld im Formular umzubauen, ist mühsam. Der Dialog fragt alles in einem Schritt: die Form
// (Matrix, Strip, 3D-Gitter), die Anzahl der LEDs je Achse (x y z), den Abstand, die Lage im Raum, die
// Verkabelung (Zählrichtung, Serpentine, Startecke) und den Patch (Universe und Kanal). Eine Zeile unter den
// Feldern nennt Größe, LED-Zahl und Kanäle, damit man vor dem Anlegen sieht, was entsteht.
//
// Der Dialog ändert nichts am Projekt; er liefert ein NewFixture, das LayoutPanel::addFixture anlegt.
#pragma once

#include <QDialog>
#include <QString>

#include <vector>

#include "form_helpers.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/output/output_plan.hpp"
#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QStackedWidget;

namespace whitehole::studio {

enum class PatchMode { Auto, Manual, None };

/// Was der Dialog anlegen will. Ein leerer Name heißt: das Layout vergibt einen (z. B. "Cube 1").
struct NewFixture {
    QString name;
    Vec3 origin;
    mapping::FixtureShape shape{mapping::MatrixShape{}};
    PatchMode patchMode{PatchMode::Auto};
    output::FixturePatch patch;  // nur bei PatchMode::Manual
};

class AddFixtureDialog : public QDialog {
    Q_OBJECT

public:
    AddFixtureDialog(runtime::Player& player, const Vec3& origin, QWidget* parent);

    /// Die Eingaben, wie die Felder sie gerade zeigen.
    [[nodiscard]] NewFixture result() const;

    /// Rauchtest für die CI: füllt Matrix, Strip und Gitter wie ein Mensch aus und prüft result() und die
    /// Anzeige. Danach steht ein 4×3×2-Gitter mit Startecke und automatischem Patch im Dialog. Leer bei Erfolg.
    QString exercise();

private:
    QWidget* buildMatrixPage();
    QWidget* buildStripPage();
    QWidget* buildGridPage();
    QWidget* buildPatchBox();
    [[nodiscard]] mapping::FixtureShape readShape() const;
    [[nodiscard]] output::FixturePatch readPatch() const;
    void refresh();
    void syncPatchFields();
    void onPatchMode(int mode);

    runtime::Player& player_;
    bool ready_{false};
    QLineEdit* name_{nullptr};
    form::Vec3Boxes origin_;
    QComboBox* kind_{nullptr};
    QStackedWidget* pages_{nullptr};
    // Matrix
    QSpinBox* columns_{nullptr};
    QSpinBox* rows_{nullptr};
    form::Vec2Boxes matrixPitch_;
    QComboBox* corner_{nullptr};
    QComboBox* firstAxis_{nullptr};
    QCheckBox* matrixSerpentine_{nullptr};
    // Strip
    QSpinBox* count_{nullptr};
    form::Vec3Boxes start_;
    form::Vec3Boxes end_;
    // Gitter
    QSpinBox* nx_{nullptr};
    QSpinBox* ny_{nullptr};
    QSpinBox* nz_{nullptr};
    form::Vec3Boxes gridPitch_;
    QComboBox* order_{nullptr};
    QCheckBox* gridSerpentine_{nullptr};
    QCheckBox* flipX_{nullptr};
    QCheckBox* flipY_{nullptr};
    QCheckBox* flipZ_{nullptr};
    // Patch
    QComboBox* patchMode_{nullptr};
    QLabel* autoInfo_{nullptr};
    QComboBox* protocol_{nullptr};
    QSpinBox* destination_{nullptr};
    QSpinBox* universe_{nullptr};
    QSpinBox* channel_{nullptr};
    QSpinBox* ddpOffset_{nullptr};
    QComboBox* colorOrder_{nullptr};
    std::vector<QWidget*> manualFields_;
    QLabel* info_{nullptr};
    QDialogButtonBox* buttons_{nullptr};
};

}  // namespace whitehole::studio
