// fixture_form.cpp: Formular für ein Fixture.
#include "fixture_form.hpp"

#include <QFormLayout>
#include <QTimer>

#include <cmath>
#include <variant>

namespace whitehole::studio {

namespace {

using form::combo;
using form::doubleBox;
using form::fromStd;
using form::intBox;

constexpr double kBig = 1.0e6;

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

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

    QStringList typeNames{tr("(eigene Form)")};
    int typeIndex = 0;
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        typeNames.push_back(fromStd(type.name));
        if (type.name == fixture->type) {
            typeIndex = static_cast<int>(typeNames.size()) - 1;
        }
    }
    c_.type = combo(typeNames, typeIndex);
    c_.type->setToolTip(tr("Bauform aus der Bibliothek (Stage Config → Fixtures). Änderungen an der Bauform gelten für alle Exemplare."));
    connect(c_.type, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { applyType(); });
    layout->addRow(tr("Bauform"), form::labeledRow({{QString{}, c_.type}}));

    if (!fixture->type.empty()) {
        layout->addRow(new QLabel{tr("Die Form kommt aus der Bauform und wird im Reiter Fixtures geändert.")});
    } else {
        buildShapeFields(*layout, fixture->shape);
    }
}

void FixtureForm::buildShapeFields(QFormLayout& layout, const mapping::FixtureShape& shape) {
    const auto changed = [this]() { applyShape(); };
    const auto onInt = [this, changed](QSpinBox* box) {
        connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [changed](int) { changed(); });
    };
    const auto onDouble = [this, changed](QDoubleSpinBox* box) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [changed](double) { changed(); });
    };
    const auto onCombo = [this, changed](QComboBox* box) {
        connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, [changed](int) { changed(); });
    };
    const auto onCheck = [this, changed](QCheckBox* box) {
        connect(box, &QCheckBox::toggled, this, [changed](bool) { changed(); });
    };

    std::visit(
        Overloaded{
            [&](const mapping::MatrixShape& m) {
                layout.addRow(new QLabel{tr("<b>Matrix</b>")});
                c_.columns = intBox(static_cast<int>(m.columns), 1, 4096);
                c_.rows = intBox(static_cast<int>(m.rows), 1, 4096);
                c_.matrixPitch = form::vec2Boxes(m.pitch, 0.001, kBig);
                c_.corner = combo({tr("oben links"), tr("oben rechts"), tr("unten links"), tr("unten rechts")},
                                  static_cast<int>(m.wiring.start));
                c_.firstAxis = combo({tr("Zeilen"), tr("Spalten")}, static_cast<int>(m.wiring.first));
                c_.matrixSerpentine = new QCheckBox{tr("Serpentine")};
                c_.matrixSerpentine->setChecked(m.wiring.serpentine);
                onInt(c_.columns);
                onInt(c_.rows);
                onDouble(c_.matrixPitch.x);
                onDouble(c_.matrixPitch.y);
                onCombo(c_.corner);
                onCombo(c_.firstAxis);
                onCheck(c_.matrixSerpentine);
                layout.addRow(tr("Größe"),
                              form::labeledRow({{tr("Spalten"), c_.columns}, {tr("Zeilen"), c_.rows}}));
                layout.addRow(tr("Abstand x y"), c_.matrixPitch.widget());
                layout.addRow(tr("Kabel beginnt"),
                              form::labeledRow({{QString{}, c_.corner}, {tr("Zuerst entlang"), c_.firstAxis}}));
                layout.addRow(QString{}, c_.matrixSerpentine);
            },
            [&](const mapping::StripShape& s) {
                layout.addRow(new QLabel{tr("<b>Strip</b>")});
                c_.count = intBox(static_cast<int>(s.count), 1, 1000000);
                c_.start = form::vec3Boxes(s.start, -kBig, kBig);
                c_.end = form::vec3Boxes(s.end, -kBig, kBig);
                onInt(c_.count);
                for (QDoubleSpinBox* box : {c_.start.x, c_.start.y, c_.start.z, c_.end.x, c_.end.y, c_.end.z}) {
                    onDouble(box);
                }
                layout.addRow(tr("LEDs"), form::labeledRow({{QString{}, c_.count}}));
                layout.addRow(tr("Anfang x y z"), c_.start.widget());
                layout.addRow(tr("Ende x y z"), c_.end.widget());
            },
            [&](const mapping::GridShape& g) {
                layout.addRow(new QLabel{tr("<b>3D-Gitter</b>")});
                c_.nx = intBox(static_cast<int>(g.nx), 1, 1024);
                c_.ny = intBox(static_cast<int>(g.ny), 1, 1024);
                c_.nz = intBox(static_cast<int>(g.nz), 1, 1024);
                c_.gridPitch = form::vec3Boxes(g.pitch, 0.001, kBig, 0.5);
                c_.axisOrder = combo({QStringLiteral("XYZ"), QStringLiteral("XZY"), QStringLiteral("YXZ"),
                                      QStringLiteral("YZX"), QStringLiteral("ZXY"), QStringLiteral("ZYX")},
                                     static_cast<int>(g.wiring.order));
                c_.gridSerpentine = new QCheckBox{tr("Serpentine")};
                c_.gridSerpentine->setChecked(g.wiring.serpentine);
                c_.flipX = new QCheckBox{tr("rechts (x max)")};
                c_.flipY = new QCheckBox{tr("oben (y max)")};
                c_.flipZ = new QCheckBox{tr("vorn (z max)")};
                c_.flipX->setChecked(g.wiring.flipX);
                c_.flipY->setChecked(g.wiring.flipY);
                c_.flipZ->setChecked(g.wiring.flipZ);
                for (QCheckBox* box : {c_.flipX, c_.flipY, c_.flipZ}) {
                    box->setToolTip(tr("Das Kabel beginnt an dieser Seite statt an der gegenüberliegenden"));
                    onCheck(box);
                }
                onInt(c_.nx);
                onInt(c_.ny);
                onInt(c_.nz);
                for (QDoubleSpinBox* box : {c_.gridPitch.x, c_.gridPitch.y, c_.gridPitch.z}) {
                    onDouble(box);
                }
                onCombo(c_.axisOrder);
                onCheck(c_.gridSerpentine);
                layout.addRow(tr("Anzahl x y z"), form::labeledRow({{QString{}, c_.nx}, {QString{}, c_.ny}, {QString{}, c_.nz}}));
                layout.addRow(tr("Abstand x y z"), c_.gridPitch.widget());
                layout.addRow(tr("Reihenfolge (schnellste zuerst)"), form::labeledRow({{QString{}, c_.axisOrder}}));
                layout.addRow(QString{}, c_.gridSerpentine);
                layout.addRow(tr("Kabel beginnt"), form::rowOf({c_.flipX, c_.flipY, c_.flipZ}));
            },
            [&](const mapping::PointsShape& p) {
                layout.addRow(new QLabel{tr("<b>Punktwolke</b>: %1 Punkte, nur in der Datei änderbar")
                                             .arg(p.points.size())});
            },
            [&](const auto&) {  // Ring und Säule: Formular folgt mit dem neuen Stage-Config-Fenster
                layout.addRow(new QLabel{tr("<b>Ring oder Säule</b>: Form vorerst nur in der Datei änderbar")});
            }},
        shape);
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
    const std::string name = index <= 0 ? std::string{} : c_.type->currentText().toUtf8().toStdString();
    const mapping::Fixture* fixture = player_.project()->layout.find(id_);
    if (fixture == nullptr || fixture->type == name) {
        return;
    }
    report(player_.assignFixtureType(id_, name), true);
}

void FixtureForm::applyShape() {
    const mapping::Fixture* fixture = player_.project()->layout.find(id_);
    if (fixture != nullptr) {
        report(player_.setFixtureShape(id_, readShape(fixture->shape)));
    }
}

mapping::FixtureShape FixtureForm::readShape(const mapping::FixtureShape& current) const {
    if (std::get_if<mapping::MatrixShape>(&current) != nullptr && c_.columns != nullptr) {
        mapping::MatrixShape m;
        m.columns = static_cast<std::uint32_t>(c_.columns->value());
        m.rows = static_cast<std::uint32_t>(c_.rows->value());
        m.pitch = c_.matrixPitch.value();
        m.wiring.start = static_cast<mapping::StartCorner>(c_.corner->currentIndex());
        m.wiring.first = static_cast<mapping::FirstAxis>(c_.firstAxis->currentIndex());
        m.wiring.serpentine = c_.matrixSerpentine->isChecked();
        return m;
    }
    if (std::get_if<mapping::StripShape>(&current) != nullptr && c_.count != nullptr) {
        mapping::StripShape s;
        s.count = static_cast<std::uint32_t>(c_.count->value());
        s.start = c_.start.value();
        s.end = c_.end.value();
        return s;
    }
    if (std::get_if<mapping::GridShape>(&current) != nullptr && c_.nx != nullptr) {
        mapping::GridShape g;
        g.nx = static_cast<std::uint32_t>(c_.nx->value());
        g.ny = static_cast<std::uint32_t>(c_.ny->value());
        g.nz = static_cast<std::uint32_t>(c_.nz->value());
        g.pitch = c_.gridPitch.value();
        g.wiring.order = static_cast<mapping::AxisOrder>(c_.axisOrder->currentIndex());
        g.wiring.serpentine = c_.gridSerpentine->isChecked();
        g.wiring.flipX = c_.flipX->isChecked();
        g.wiring.flipY = c_.flipY->isChecked();
        g.wiring.flipZ = c_.flipZ->isChecked();
        return g;
    }
    return current;
}

}  // namespace whitehole::studio
