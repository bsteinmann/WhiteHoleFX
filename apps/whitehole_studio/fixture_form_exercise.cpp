// fixture_form_exercise.cpp: Rauchtests des Fixture-Formulars (nur für die CI, siehe MainWindow::exerciseEditor).
//
// Warum eine eigene Datei: fixture_form.cpp ist sonst über der 400-Zeilen-Grenze. Die Tests bedienen die Felder wie ein
// Mensch und prüfen, dass die Änderung beim Player ankommt.
#include "fixture_form.hpp"

#include <cmath>

namespace whitehole::studio {

QString FixtureForm::exercise() {
    const mapping::Fixture* fixture = player_.project() != nullptr ? player_.project()->layout.find(id_) : nullptr;
    if (fixture == nullptr || c_.origin.x == nullptr || c_.type == nullptr) {
        return QStringLiteral("Formular: kein Fixture angezeigt");
    }
    // Ursprung ändern, wie es ein Mensch im Zahlenfeld täte
    const double x0 = fixture->origin.x;
    c_.origin.x->setValue(x0 + 5.0);
    fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr || std::abs(static_cast<double>(fixture->origin.x) - (x0 + 5.0)) > 1.0e-3) {
        return QStringLiteral("Formular: Ursprung wurde nicht übernommen");
    }
    // "Typ wird Pflicht" (siehe CLAUDE.md): auch dieser frisch angelegte Strip hat sofort eine eigene
    // Bauform; die Form selbst lässt sich hier nicht mehr bearbeiten (siehe fixture_form.hpp).
    if (fixture->type.empty() || c_.type->currentText().toUtf8().toStdString() != fixture->type) {
        return QStringLiteral("Formular: Fixture zeigt keine (oder die falsche) Bauform an");
    }
    return {};
}

QString FixtureForm::exerciseTyped() {
    const mapping::Fixture* fixture = player_.project() != nullptr ? player_.project()->layout.find(id_) : nullptr;
    if (fixture == nullptr || c_.rotation.x == nullptr || c_.type == nullptr) {
        return QStringLiteral("Formular: kein Fixture mit Drehung angezeigt");
    }
    if (fixture->type.empty()) {
        return QStringLiteral("Formular: Fixture aus einer Bauform zeigt keine Bauform");
    }
    // Drehung um alle drei Achsen über die Felder
    c_.rotation.y->setValue(45.0);
    c_.rotation.z->setValue(30.0);
    fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr || std::abs(static_cast<double>(fixture->rotation.y) - 45.0) > 1.0e-3 ||
        std::abs(static_cast<double>(fixture->rotation.z) - 30.0) > 1.0e-3) {
        return QStringLiteral("Formular: Drehung wurde nicht übernommen");
    }
    // "Typ wird Pflicht": kein Eintrag zum Loslösen mehr - nur noch Bauformen plus "Neue Bauform …" als
    // letzten Eintrag (den tatsächlich auszulösen bräuchte einen echten QInputDialog, der den Rauchtest
    // ohne Publikum blockieren würde, siehe exerciseRename in node_editor_exercise_rename.cpp - darum hier
    // nur die Struktur der Liste geprüft).
    if (c_.type->itemText(c_.type->count() - 1) != tr("Neue Bauform aus dieser Form …") ||
        c_.type->currentText().toUtf8().toStdString() != fixture->type) {
        return QStringLiteral("Formular: Bauform-Liste zeigt die Bauform des Fixtures nicht an");
    }
    return {};
}

}  // namespace whitehole::studio
