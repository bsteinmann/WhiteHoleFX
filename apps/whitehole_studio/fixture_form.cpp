// fixture_form.cpp: Formular für ein Fixture.
#include "fixture_form.hpp"

#include <QFormLayout>
#include <QInputDialog>
#include <QTimer>

namespace whitehole::studio {

namespace {

using form::combo;
using form::fromStd;

constexpr double kBig = 1.0e6;

}  // namespace

FixtureForm::FixtureForm(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    outer_ = new QVBoxLayout{this};
    outer_->setContentsMargins(0, 0, 0, 0);
}

void FixtureForm::showFixture(mapping::FixtureId id) {
    id_ = id;
    build();
}

void FixtureForm::build() {
    if (content_ != nullptr) {
        outer_->removeWidget(content_);
        content_->hide();
        content_->deleteLater();  // wir können aus einem Signal dieses Inhalts hierher kommen
        content_ = nullptr;
    }
    c_ = Controls{};

    const project::Project* project = player_.project();
    const mapping::Fixture* fixture = project != nullptr ? project->layout.find(id_) : nullptr;
    content_ = new QWidget{this};
    outer_->addWidget(content_);
    auto* layout = new QFormLayout{content_};
    if (fixture == nullptr) {
        layout->addRow(new QLabel{tr("Kein Fixture ausgewählt.")});
        return;
    }

    c_.name = new QLineEdit{fromStd(fixture->name)};
    connect(c_.name, &QLineEdit::editingFinished, this, [this]() { applyName(); });
    layout->addRow(tr("Name"), c_.name);

    c_.origin = form::vec3Boxes(fixture->origin, -kBig, kBig);
    for (QDoubleSpinBox* box : {c_.origin.x, c_.origin.y, c_.origin.z}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { applyOrigin(); });
    }
    layout->addRow(tr("Ursprung x y z"), c_.origin.widget());

    c_.rotation = form::vec3Boxes(fixture->rotation, -360.0, 360.0, 5.0);
    for (QDoubleSpinBox* box : {c_.rotation.x, c_.rotation.y, c_.rotation.z}) {
        box->setDecimals(1);
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { applyRotation(); });
    }
    c_.rotation.x->setToolTip(tr("Drehung um die x-Achse in Grad (um die Mitte des Fixtures)"));
    c_.rotation.y->setToolTip(tr("Drehung um die y-Achse in Grad"));
    c_.rotation.z->setToolTip(tr("Drehung um die z-Achse in Grad"));
    layout->addRow(tr("Drehung x y z (Grad)"), c_.rotation.widget());

    // "Typ wird Pflicht" (siehe CLAUDE.md): die Form selbst ist hier nicht mehr editierbar, nur noch, welches
    // Fixture gilt. Der letzte Eintrag legt aus der aktuellen Form ein neues Fixture an - der einzige Weg,
    // ein Fixture ohne eigenes (ältere Projektdatei) wieder einem Fixture der Show zuzuordnen.
    QStringList typeNames;
    int typeIndex = -1;
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        typeNames.push_back(fromStd(type.name));
        if (type.name == fixture->type) {
            typeIndex = static_cast<int>(typeNames.size()) - 1;
        }
    }
    typeNames.push_back(tr("Neues Fixture aus dieser Form …"));
    const int newTypeIndex = static_cast<int>(typeNames.size()) - 1;
    c_.type = combo(typeNames, typeIndex >= 0 ? typeIndex : newTypeIndex);
    c_.type->setToolTip(tr("Fixture aus dem Reiter Fixtures (Stage Config). Änderungen daran gelten für alle Exemplare."));
    connect(c_.type, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { applyType(); });
    layout->addRow(tr("Fixture"), form::labeledRow({{QString{}, c_.type}}));

    if (!fixture->type.empty()) {
        layout->addRow(new QLabel{tr("Die Form kommt vom Fixture und wird im Reiter Fixtures geändert.")});
    } else {
        // Typlos: ältere Projektdatei, oder das zugewiesene Fixture wurde im Reiter Fixtures entfernt
        // (Player::removeFixtureType - Exemplare behalten dann ihre Form als eigene).
        layout->addRow(new QLabel{tr("Dieses Fixture ist mit keinem Fixture aus dem Reiter Fixtures verknüpft "
                                     "(ältere Projektdatei oder es wurde entfernt). Über \"Fixture\" → \"Neues "
                                     "Fixture aus dieser Form …\" lässt sich wieder eines anlegen.")});
    }
}

void FixtureForm::report(const Result<void>& result, bool rebuildAfter) {
    if (result.ok()) {
        Q_EMIT edited();
        if (rebuildAfter) {
            QTimer::singleShot(0, this, [this]() { build(); });
        }
        return;
    }
    Q_EMIT failed(fromStd(result.error().message));
    // Anzeige auf den gespeicherten Wert zurücksetzen (nicht synchron: der Sender lebt im Inhalt)
    QTimer::singleShot(0, this, [this]() { build(); });
}

void FixtureForm::applyName() {
    if (c_.name == nullptr) {
        return;
    }
    const std::string name = c_.name->text().toUtf8().toStdString();
    const mapping::Fixture* fixture = player_.project()->layout.find(id_);
    if (fixture != nullptr && fixture->name == name) {
        return;
    }
    report(player_.renameFixture(id_, name));
}

void FixtureForm::applyOrigin() { report(player_.moveFixture(id_, c_.origin.value())); }

void FixtureForm::applyRotation() { report(player_.setFixtureRotation(id_, c_.rotation.value())); }

void FixtureForm::applyType() {
    const int index = c_.type->currentIndex();
    const int newTypeIndex = c_.type->count() - 1;
    const mapping::Fixture* fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr) {
        return;
    }
    if (index == newTypeIndex) {
        // "Neues Fixture aus dieser Form …": deckt sowohl den Fork eines schon zugewiesenen Fixtures als
        // auch das Nachrüsten eines typlosen Fixtures aus einer älteren Projektdatei ab.
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("Neues Fixture"), tr("Name:"), QLineEdit::Normal,
                                                     QString{}, &accepted)
                                 .trimmed();
        if (!accepted || name.isEmpty()) {
            build();  // Abbruch: Combo stand auf dem neuen Eintrag, Anzeige zurücksetzen
            return;
        }
        const std::string typeName = name.toUtf8().toStdString();
        const Result<void> added = player_.addFixtureType(typeName, fixture->shape);
        if (!added.ok()) {
            report(added);
            return;
        }
        report(player_.assignFixtureType(id_, typeName), true);
        return;
    }
    const std::string name = c_.type->currentText().toUtf8().toStdString();
    if (fixture->type == name) {
        return;
    }
    report(player_.assignFixtureType(id_, name), true);
}

}  // namespace whitehole::studio
