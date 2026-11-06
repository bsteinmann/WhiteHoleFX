// fixture_form_exercise.cpp: Rauchtests des Fixture-Formulars (nur für die CI, siehe MainWindow::exerciseEditor).
//
// Warum eine eigene Datei: fixture_form.cpp ist sonst über der 400-Zeilen-Grenze. Die Tests bedienen die Felder wie ein
// Mensch und prüfen, dass die Änderung beim Player ankommt.
#include "fixture_form.hpp"

#include <cmath>

namespace whitehole::studio {

QString FixtureForm::exercise() {
    const mapping::Fixture* fixture = player_.project() != nullptr ? player_.project()->layout.find(id_) : nullptr;
    if (fixture == nullptr || c_.origin.x == nullptr || c_.count == nullptr || c_.patched == nullptr) {
        return QStringLiteral("Formular: kein Strip angezeigt");
    }
    // Ursprung ändern, wie es ein Mensch im Zahlenfeld täte
    const double x0 = fixture->origin.x;
    c_.origin.x->setValue(x0 + 5.0);
    fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr || std::abs(static_cast<double>(fixture->origin.x) - (x0 + 5.0)) > 1.0e-3) {
        return QStringLiteral("Formular: Ursprung wurde nicht übernommen");
    }
    // Größe ändern: die Zahl der LEDs ändert sich mit
    const std::uint32_t before = player_.status().ledCount;
    c_.count->setValue(c_.count->value() + 10);
    if (player_.status().ledCount != before + 10) {
        return QStringLiteral("Formular: LED-Zahl %1 statt %2").arg(player_.status().ledCount).arg(before + 10);
    }
    // Farbreihenfolge im Patch ändern
    c_.colorOrder->setCurrentIndex(2);
    const output::FixturePatch* patch = patchOf(id_);
    if (patch == nullptr || patch->color.order != output::ColorOrder::GRB) {
        return QStringLiteral("Formular: Farbreihenfolge wurde nicht übernommen");
    }
    return {};
}

QString FixtureForm::exerciseTyped() {
    const mapping::Fixture* fixture = player_.project() != nullptr ? player_.project()->layout.find(id_) : nullptr;
    if (fixture == nullptr || c_.rotation.x == nullptr || c_.type == nullptr) {
        return QStringLiteral("Formular: kein Fixture mit Drehung angezeigt");
    }
    if (fixture->type.empty() || c_.columns != nullptr || c_.count != nullptr || c_.nx != nullptr) {
        return QStringLiteral("Formular: Fixture aus einer Bauform zeigt trotzdem Formfelder");
    }
    // Drehung um alle drei Achsen über die Felder
    c_.rotation.y->setValue(45.0);
    c_.rotation.z->setValue(30.0);
    fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr || std::abs(static_cast<double>(fixture->rotation.y) - 45.0) > 1.0e-3 ||
        std::abs(static_cast<double>(fixture->rotation.z) - 30.0) > 1.0e-3) {
        return QStringLiteral("Formular: Drehung wurde nicht übernommen");
    }
    // Von der Bauform lösen: die Form bleibt als eigene stehen
    c_.type->setCurrentIndex(0);
    fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr || !fixture->type.empty()) {
        return QStringLiteral("Formular: Fixture lässt sich nicht von der Bauform lösen");
    }
    return {};
}

}  // namespace whitehole::studio
