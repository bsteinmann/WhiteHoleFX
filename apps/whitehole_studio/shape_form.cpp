// shape_form.cpp: Umsetzung zu shape_form.hpp. Felder je Form und das Zurücklesen in eine Form.
#include "shape_form.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <variant>

namespace whitehole::studio {

namespace {

using form::combo;
using form::intBox;

constexpr double kBig = 1.0e6;

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace

ShapeForm::ShapeForm(QWidget* parent) : QWidget{parent} {
    outer_ = new QVBoxLayout{this};
    outer_->setContentsMargins(0, 0, 0, 0);
    build();
}

void ShapeForm::setShape(const mapping::FixtureShape& shape) {
    shape_ = shape;
    build();
}

void ShapeForm::build() {
    delete content_;
    c_ = Controls{};
    content_ = new QWidget{this};
    auto* layout = new QFormLayout{content_};
    layout->setContentsMargins(0, 0, 0, 0);
    std::visit(Overloaded{[&](const mapping::MatrixShape& m) { buildMatrix(*layout, m); },
                          [&](const mapping::StripShape& s) { buildStrip(*layout, s); },
                          [&](const mapping::GridShape& g) { buildGrid(*layout, g); },
                          [&](const mapping::RingShape& r) { buildRing(*layout, r); },
                          [&](const mapping::HelixShape& h) { buildHelix(*layout, h); },
                          [&](const mapping::PointsShape& p) {
                              layout->addRow(new QLabel{tr("<b>Punktwolke</b>: %1 Punkte, nur in der Datei änderbar")
                                                            .arg(p.points.size())});
                          }},
               shape_);
    outer_->addWidget(content_);
}

void ShapeForm::changed() {
    shape_ = read();
    Q_EMIT shapeEdited(shape_);
}

void ShapeForm::buildMatrix(QFormLayout& layout, const mapping::MatrixShape& m) {
    layout.addRow(new QLabel{tr("<b>2D-Matrix</b>")});
    c_.columns = intBox(static_cast<int>(m.columns), 1, 4096);
    c_.rows = intBox(static_cast<int>(m.rows), 1, 4096);
    c_.matrixPitch = form::vec2Boxes(m.pitch, 0.001, kBig);
    c_.corner = combo({tr("oben links"), tr("oben rechts"), tr("unten links"), tr("unten rechts")},
                      static_cast<int>(m.wiring.start));
    c_.firstAxis = combo({tr("Zeilen"), tr("Spalten")}, static_cast<int>(m.wiring.first));
    c_.matrixSerpentine = new QCheckBox{tr("Serpentine")};
    c_.matrixSerpentine->setChecked(m.wiring.serpentine);
    connect(c_.columns, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { changed(); });
    connect(c_.rows, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { changed(); });
    for (QDoubleSpinBox* box : {c_.matrixPitch.x, c_.matrixPitch.y}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { changed(); });
    }
    for (QComboBox* box : {c_.corner, c_.firstAxis}) {
        connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { changed(); });
    }
    connect(c_.matrixSerpentine, &QCheckBox::toggled, this, [this](bool) { changed(); });
    layout.addRow(tr("Größe"), form::labeledRow({{tr("Spalten"), c_.columns}, {tr("Zeilen"), c_.rows}}));
    layout.addRow(tr("Abstand x y"), c_.matrixPitch.widget());
    layout.addRow(tr("Kabel beginnt"),
                  form::labeledRow({{QString{}, c_.corner}, {tr("Zuerst entlang"), c_.firstAxis}}));
    layout.addRow(QString{}, c_.matrixSerpentine);
}

void ShapeForm::buildStrip(QFormLayout& layout, const mapping::StripShape& s) {
    layout.addRow(new QLabel{tr("<b>Strip</b>")});
    c_.count = intBox(static_cast<int>(s.count), 1, 1000000);
    c_.start = form::vec3Boxes(s.start, -kBig, kBig);
    c_.end = form::vec3Boxes(s.end, -kBig, kBig);
    connect(c_.count, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { changed(); });
    for (QDoubleSpinBox* box : {c_.start.x, c_.start.y, c_.start.z, c_.end.x, c_.end.y, c_.end.z}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { changed(); });
    }
    layout.addRow(tr("LEDs"), form::labeledRow({{QString{}, c_.count}}));
    layout.addRow(tr("Anfang x y z"), c_.start.widget());
    layout.addRow(tr("Ende x y z"), c_.end.widget());
}

void ShapeForm::buildGrid(QFormLayout& layout, const mapping::GridShape& g) {
    layout.addRow(new QLabel{tr("<b>3D-Matrix</b>")});
    c_.nx = intBox(static_cast<int>(g.nx), 1, 1024);
    c_.ny = intBox(static_cast<int>(g.ny), 1, 1024);
    c_.nz = intBox(static_cast<int>(g.nz), 1, 1024);
    c_.gridPitch = form::vec3Boxes(g.pitch, 0.001, kBig, 0.5);
    c_.axisOrder = combo({QStringLiteral("XYZ"), QStringLiteral("XZY"), QStringLiteral("YXZ"), QStringLiteral("YZX"),
                          QStringLiteral("ZXY"), QStringLiteral("ZYX")},
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
        connect(box, &QCheckBox::toggled, this, [this](bool) { changed(); });
    }
    for (QSpinBox* box : {c_.nx, c_.ny, c_.nz}) {
        connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { changed(); });
    }
    for (QDoubleSpinBox* box : {c_.gridPitch.x, c_.gridPitch.y, c_.gridPitch.z}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { changed(); });
    }
    connect(c_.axisOrder, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { changed(); });
    connect(c_.gridSerpentine, &QCheckBox::toggled, this, [this](bool) { changed(); });
    layout.addRow(tr("Anzahl x y z"), form::labeledRow({{QString{}, c_.nx}, {QString{}, c_.ny}, {QString{}, c_.nz}}));
    layout.addRow(tr("Abstand x y z"), c_.gridPitch.widget());
    layout.addRow(tr("Reihenfolge (schnellste zuerst)"), form::labeledRow({{QString{}, c_.axisOrder}}));
    layout.addRow(QString{}, c_.gridSerpentine);
    layout.addRow(tr("Kabel beginnt"), form::rowOf({c_.flipX, c_.flipY, c_.flipZ}));
}

void ShapeForm::buildRing(QFormLayout& layout, const mapping::RingShape& r) {
    layout.addRow(new QLabel{tr("<b>Kreis</b>")});
    c_.count = intBox(static_cast<int>(r.count), 1, 1000000);
    c_.radius = form::doubleBox(r.radius, 0.001, kBig, 0.5);
    c_.startAngle = form::doubleBox(r.startAngle, -360.0, 360.0, 15.0, 1);
    c_.clockwise = new QCheckBox{tr("im Uhrzeigersinn")};
    c_.clockwise->setChecked(r.clockwise);
    c_.startAngle->setToolTip(tr("Winkel der ersten LED: 0 = rechts, 90 = oben"));
    connect(c_.count, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { changed(); });
    for (QDoubleSpinBox* box : {c_.radius, c_.startAngle}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { changed(); });
    }
    connect(c_.clockwise, &QCheckBox::toggled, this, [this](bool) { changed(); });
    layout.addRow(tr("LEDs"), form::labeledRow({{QString{}, c_.count}}));
    layout.addRow(tr("Radius"), form::labeledRow({{QString{}, c_.radius}}));
    layout.addRow(tr("Erste LED bei (Grad)"), form::labeledRow({{QString{}, c_.startAngle}}));
    layout.addRow(QString{}, c_.clockwise);
}

void ShapeForm::buildHelix(QFormLayout& layout, const mapping::HelixShape& h) {
    layout.addRow(new QLabel{tr("<b>Säule</b>")});
    c_.count = intBox(static_cast<int>(h.count), 1, 1000000);
    c_.radius = form::doubleBox(h.radius, 0.001, kBig, 0.5);
    c_.height = form::doubleBox(h.height, 0.001, kBig, 1.0);
    c_.turns = form::doubleBox(h.turns, 0.0, 10000.0, 0.5, 2);
    c_.startAngle = form::doubleBox(h.startAngle, -360.0, 360.0, 15.0, 1);
    c_.clockwise = new QCheckBox{tr("im Uhrzeigersinn")};
    c_.clockwise->setChecked(h.clockwise);
    c_.turns->setToolTip(tr("Wie oft die Spirale um die Säule läuft (0 = gerade nach oben)"));
    connect(c_.count, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { changed(); });
    for (QDoubleSpinBox* box : {c_.radius, c_.height, c_.turns, c_.startAngle}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { changed(); });
    }
    connect(c_.clockwise, &QCheckBox::toggled, this, [this](bool) { changed(); });
    layout.addRow(tr("LEDs"), form::labeledRow({{QString{}, c_.count}}));
    layout.addRow(tr("Radius und Höhe"), form::labeledRow({{QString{}, c_.radius}, {QString{}, c_.height}}));
    layout.addRow(tr("Windungen"), form::labeledRow({{QString{}, c_.turns}}));
    layout.addRow(tr("Erste LED bei (Grad)"), form::labeledRow({{QString{}, c_.startAngle}}));
    layout.addRow(QString{}, c_.clockwise);
}

mapping::FixtureShape ShapeForm::read() const {
    if (std::get_if<mapping::MatrixShape>(&shape_) != nullptr && c_.columns != nullptr) {
        mapping::MatrixShape m;
        m.columns = static_cast<std::uint32_t>(c_.columns->value());
        m.rows = static_cast<std::uint32_t>(c_.rows->value());
        m.pitch = c_.matrixPitch.value();
        m.wiring.start = static_cast<mapping::StartCorner>(c_.corner->currentIndex());
        m.wiring.first = static_cast<mapping::FirstAxis>(c_.firstAxis->currentIndex());
        m.wiring.serpentine = c_.matrixSerpentine->isChecked();
        return m;
    }
    if (std::get_if<mapping::StripShape>(&shape_) != nullptr && c_.count != nullptr) {
        mapping::StripShape s;
        s.count = static_cast<std::uint32_t>(c_.count->value());
        s.start = c_.start.value();
        s.end = c_.end.value();
        return s;
    }
    if (std::get_if<mapping::GridShape>(&shape_) != nullptr && c_.nx != nullptr) {
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
    if (std::get_if<mapping::RingShape>(&shape_) != nullptr && c_.radius != nullptr) {
        mapping::RingShape r;
        r.count = static_cast<std::uint32_t>(c_.count->value());
        r.radius = static_cast<float>(c_.radius->value());
        r.startAngle = static_cast<float>(c_.startAngle->value());
        r.clockwise = c_.clockwise->isChecked();
        return r;
    }
    if (std::get_if<mapping::HelixShape>(&shape_) != nullptr && c_.height != nullptr) {
        mapping::HelixShape h;
        h.count = static_cast<std::uint32_t>(c_.count->value());
        h.radius = static_cast<float>(c_.radius->value());
        h.height = static_cast<float>(c_.height->value());
        h.turns = static_cast<float>(c_.turns->value());
        h.startAngle = static_cast<float>(c_.startAngle->value());
        h.clockwise = c_.clockwise->isChecked();
        return h;
    }
    return shape_;
}

QString ShapeForm::exercise() {
    int reported = 0;
    std::uint64_t leds = 0;
    const QMetaObject::Connection link = connect(this, &ShapeForm::shapeEdited, this, [&](const mapping::FixtureShape& s) {
        ++reported;
        leds = mapping::ledCount(s);
    });
    QSpinBox* box = c_.columns != nullptr ? c_.columns : (c_.count != nullptr ? c_.count : c_.nx);
    if (box == nullptr) {
        disconnect(link);
        return QStringLiteral("Formularfeld für die Form: keine Größe zum Ändern");
    }
    const int before = box->value();
    box->setValue(before + 1);
    disconnect(link);
    if (reported < 1 || leds == 0) {
        return QStringLiteral("Formularfeld für die Form: Änderung wurde nicht gemeldet");
    }
    return {};
}

}  // namespace whitehole::studio
