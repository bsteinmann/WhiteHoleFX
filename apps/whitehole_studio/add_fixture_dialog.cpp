// add_fixture_dialog.cpp: Umsetzung zu add_fixture_dialog.hpp. Felder je Form, Anzeige der Größe, Patch-Wahl.
#include "add_fixture_dialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <functional>
#include <variant>

namespace whitehole::studio {

namespace {

using form::combo;
using form::fromStd;
using form::intBox;

constexpr double kBig = 1.0e6;

// Jede Änderung eines Feldes lässt die Anzeige neu rechnen.
void watch(QSpinBox* box, QObject* context, const std::function<void()>& changed) {
    QObject::connect(box, qOverload<int>(&QSpinBox::valueChanged), context, [changed](int) { changed(); });
}
void watch(QDoubleSpinBox* box, QObject* context, const std::function<void()>& changed) {
    QObject::connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), context, [changed](double) { changed(); });
}
void watch(QComboBox* box, QObject* context, const std::function<void()>& changed) {
    QObject::connect(box, qOverload<int>(&QComboBox::currentIndexChanged), context, [changed](int) { changed(); });
}
void watch(QCheckBox* box, QObject* context, const std::function<void()>& changed) {
    QObject::connect(box, &QCheckBox::toggled, context, [changed](bool) { changed(); });
}

QString number(double value) { return QString::number(value, 'g', 5); }

}  // namespace

AddFixtureDialog::AddFixtureDialog(runtime::Player& player, const Vec3& origin, QWidget* parent)
    : QDialog{parent}, player_{player} {
    setWindowTitle(tr("Fixture hinzufügen"));
    auto* box = new QVBoxLayout{this};

    auto* top = new QFormLayout;
    name_ = new QLineEdit;
    name_->setPlaceholderText(tr("automatisch (Matrix 1, Cube 1 …)"));
    origin_ = form::vec3Boxes(origin, -kBig, kBig);
    kind_ = combo({tr("Matrix (Fläche)"), tr("Strip (Linie)"), tr("3D-Gitter (Cube, Vorhang, Säule)")}, 0);
    top->addRow(tr("Name"), name_);
    top->addRow(tr("Form"), kind_);
    top->addRow(tr("Position x y z"), origin_.widget());
    box->addLayout(top);

    pages_ = new QStackedWidget;
    pages_->addWidget(buildMatrixPage());
    pages_->addWidget(buildStripPage());
    pages_->addWidget(buildGridPage());
    box->addWidget(pages_);
    box->addWidget(buildPatchBox());

    info_ = new QLabel;
    info_->setWordWrap(true);
    box->addWidget(info_);
    buttons_ = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("Hinzufügen"));
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    box->addWidget(buttons_);

    const std::function<void()> changed = [this]() { refresh(); };
    watch(kind_, this, [this]() {
        pages_->setCurrentIndex(kind_->currentIndex());
        refresh();
    });
    for (QSpinBox* spin : {columns_, rows_, count_, nx_, ny_, nz_}) {
        watch(spin, this, changed);
    }
    for (QDoubleSpinBox* spin : {matrixPitch_.x, matrixPitch_.y, start_.x, start_.y, start_.z, end_.x, end_.y, end_.z,
                                 gridPitch_.x, gridPitch_.y, gridPitch_.z}) {
        watch(spin, this, changed);
    }
    for (QComboBox* choice : {corner_, firstAxis_, order_}) {
        watch(choice, this, changed);
    }
    for (QCheckBox* check : {matrixSerpentine_, gridSerpentine_, flipX_, flipY_, flipZ_}) {
        watch(check, this, changed);
    }
    ready_ = true;
    refresh();
}

QWidget* AddFixtureDialog::buildMatrixPage() {
    auto* page = new QWidget;
    auto* fields = new QFormLayout{page};
    columns_ = intBox(16, 1, 4096);
    rows_ = intBox(8, 1, 4096);
    matrixPitch_ = form::vec2Boxes(Vec2{1.0F, 1.0F}, 0.001, kBig);
    corner_ = combo({tr("oben links"), tr("oben rechts"), tr("unten links"), tr("unten rechts")}, 0);
    firstAxis_ = combo({tr("Zeilen (waagrecht)"), tr("Spalten (senkrecht)")}, 0);
    matrixSerpentine_ = new QCheckBox{tr("Serpentine (jede zweite Zeile rückwärts)")};
    matrixSerpentine_->setChecked(true);
    fields->addRow(tr("LEDs breit × hoch"), form::rowOf({columns_, rows_}));
    fields->addRow(tr("Abstand x y"), matrixPitch_.widget());
    fields->addRow(tr("Kabel beginnt"), corner_);
    fields->addRow(tr("Zuerst entlang"), firstAxis_);
    fields->addRow(QString{}, matrixSerpentine_);
    return page;
}

QWidget* AddFixtureDialog::buildStripPage() {
    auto* page = new QWidget;
    auto* fields = new QFormLayout{page};
    count_ = intBox(60, 1, 1000000);
    start_ = form::vec3Boxes(Vec3{}, -kBig, kBig);
    end_ = form::vec3Boxes(Vec3{60.0F, 0.0F, 0.0F}, -kBig, kBig);
    fields->addRow(tr("LEDs"), count_);
    fields->addRow(tr("Anfang x y z"), start_.widget());
    fields->addRow(tr("Ende x y z"), end_.widget());
    fields->addRow(new QLabel{tr("Anfang und Ende relativ zur Position; die erste LED sitzt am Anfang.")});
    return page;
}

QWidget* AddFixtureDialog::buildGridPage() {
    auto* page = new QWidget;
    auto* fields = new QFormLayout{page};
    nx_ = intBox(8, 1, 1024);
    ny_ = intBox(8, 1, 1024);
    nz_ = intBox(8, 1, 1024);
    gridPitch_ = form::vec3Boxes(Vec3{1.0F, 1.0F, 1.0F}, 0.001, kBig, 0.5);
    order_ = combo({QStringLiteral("x → y → z"), QStringLiteral("x → z → y"), QStringLiteral("y → x → z"),
                    QStringLiteral("y → z → x"), QStringLiteral("z → x → y"), QStringLiteral("z → y → x")},
                   0);
    order_->setToolTip(tr("Erste Achse = läuft am schnellsten entlang des Kabels"));
    gridSerpentine_ = new QCheckBox{tr("Serpentine (Strang kehrt an jedem Ende um)")};
    gridSerpentine_->setChecked(true);
    flipX_ = new QCheckBox{tr("rechts (x max)")};
    flipY_ = new QCheckBox{tr("oben (y max)")};
    flipZ_ = new QCheckBox{tr("vorn (z max)")};
    for (QCheckBox* check : {flipX_, flipY_, flipZ_}) {
        check->setToolTip(tr("Das Kabel beginnt an dieser Seite statt an der gegenüberliegenden"));
    }
    fields->addRow(tr("LEDs x y z"), form::rowOf({nx_, ny_, nz_}));
    fields->addRow(tr("Abstand x y z"), gridPitch_.widget());
    fields->addRow(tr("Zählrichtung"), order_);
    fields->addRow(QString{}, gridSerpentine_);
    fields->addRow(tr("Kabel beginnt"), form::rowOf({flipX_, flipY_, flipZ_}));
    fields->addRow(new QLabel{tr("Ohne Häkchen beginnt das Kabel links, unten, hinten. x nach rechts, y nach oben, "
                               "z zum Betrachter.")});
    return page;
}

QWidget* AddFixtureDialog::buildPatchBox() {
    auto* group = new QGroupBox{tr("Patch (Ausgabe)")};
    auto* fields = new QFormLayout{group};
    patchMode_ = combo({tr("Automatisch (nächstes freies Universe)"), tr("Manuell"), tr("Nicht senden")}, 0);
    autoInfo_ = new QLabel;
    protocol_ = combo({QStringLiteral("Art-Net"), QStringLiteral("sACN"), QStringLiteral("DDP")}, 0);
    destination_ = intBox(0, 0, 255);
    universe_ = intBox(0, 0, 63999);
    channel_ = intBox(1, 1, 510);
    ddpOffset_ = intBox(0, 0, 2000000000);
    colorOrder_ = combo({QStringLiteral("RGB"), QStringLiteral("RBG"), QStringLiteral("GRB"), QStringLiteral("GBR"),
                         QStringLiteral("BRG"), QStringLiteral("BGR")},
                        0);
    manualFields_ = {protocol_, destination_, universe_, channel_, ddpOffset_, colorOrder_};
    fields->addRow(tr("Modus"), patchMode_);
    fields->addRow(QString{}, autoInfo_);
    fields->addRow(tr("Protokoll"), protocol_);
    fields->addRow(tr("Ziel Nr."), destination_);
    fields->addRow(tr("Universe"), universe_);
    fields->addRow(tr("Erster Kanal"), channel_);
    fields->addRow(tr("DDP-Offset (Byte)"), ddpOffset_);
    fields->addRow(tr("Farbreihenfolge"), colorOrder_);
    connect(patchMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int mode) { onPatchMode(mode); });
    connect(protocol_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { syncPatchFields(); });
    syncPatchFields();
    return group;
}

void AddFixtureDialog::onPatchMode(int mode) {
    if (mode == 1) {
        // Manuell beginnt mit dem Vorschlag, den "Automatisch" gewählt hätte.
        const mapping::FixtureShape shape = readShape();
        const std::uint64_t leds = mapping::validateShape(shape).ok() ? mapping::ledCount(shape) : 0;
        const output::FixturePatch suggestion = player_.suggestPatch(leds);
        universe_->setValue(suggestion.universe);
        channel_->setValue(suggestion.channel);
    }
    syncPatchFields();
}

void AddFixtureDialog::syncPatchFields() {
    const bool manual = patchMode_->currentIndex() == 1;
    const bool ddp = protocol_->currentIndex() == static_cast<int>(output::Protocol::Ddp);
    for (QWidget* field : manualFields_) {
        field->setEnabled(manual);
    }
    universe_->setEnabled(manual && !ddp);
    channel_->setEnabled(manual && !ddp);
    ddpOffset_->setEnabled(manual && ddp);
    autoInfo_->setVisible(patchMode_->currentIndex() != 1);
}

mapping::FixtureShape AddFixtureDialog::readShape() const {
    if (kind_->currentIndex() == 0) {
        mapping::MatrixShape m;
        m.columns = static_cast<std::uint32_t>(columns_->value());
        m.rows = static_cast<std::uint32_t>(rows_->value());
        m.pitch = matrixPitch_.value();
        m.wiring.start = static_cast<mapping::StartCorner>(corner_->currentIndex());
        m.wiring.first = static_cast<mapping::FirstAxis>(firstAxis_->currentIndex());
        m.wiring.serpentine = matrixSerpentine_->isChecked();
        return m;
    }
    if (kind_->currentIndex() == 1) {
        mapping::StripShape s;
        s.count = static_cast<std::uint32_t>(count_->value());
        s.start = start_.value();
        s.end = end_.value();
        return s;
    }
    mapping::GridShape g;
    g.nx = static_cast<std::uint32_t>(nx_->value());
    g.ny = static_cast<std::uint32_t>(ny_->value());
    g.nz = static_cast<std::uint32_t>(nz_->value());
    g.pitch = gridPitch_.value();
    g.wiring.order = static_cast<mapping::AxisOrder>(order_->currentIndex());
    g.wiring.serpentine = gridSerpentine_->isChecked();
    g.wiring.flipX = flipX_->isChecked();
    g.wiring.flipY = flipY_->isChecked();
    g.wiring.flipZ = flipZ_->isChecked();
    return g;
}

output::FixturePatch AddFixtureDialog::readPatch() const {
    output::FixturePatch patch;
    patch.protocol = static_cast<output::Protocol>(protocol_->currentIndex());
    patch.destination = static_cast<std::uint32_t>(destination_->value());
    patch.universe = static_cast<std::uint16_t>(universe_->value());
    patch.channel = static_cast<std::uint16_t>(channel_->value());
    patch.ddpOffset = static_cast<std::uint32_t>(ddpOffset_->value());
    patch.color.order = static_cast<output::ColorOrder>(colorOrder_->currentIndex());
    return patch;
}

NewFixture AddFixtureDialog::result() const {
    NewFixture fixture;
    fixture.name = name_->text().trimmed();
    fixture.origin = origin_.value();
    fixture.shape = readShape();
    fixture.patchMode = static_cast<PatchMode>(patchMode_->currentIndex());
    if (fixture.patchMode == PatchMode::Manual) {
        fixture.patch = readPatch();
    }
    return fixture;
}

void AddFixtureDialog::refresh() {
    if (!ready_) {
        return;
    }
    const mapping::FixtureShape shape = readShape();
    const Result<void> valid = mapping::validateShape(shape);
    QPushButton* ok = buttons_->button(QDialogButtonBox::Ok);
    ok->setEnabled(valid.ok());
    if (!valid.ok()) {
        info_->setText(tr("⚠ %1").arg(fromStd(valid.error().message)));
        return;
    }
    const std::uint64_t leds = mapping::ledCount(shape);
    const mapping::Box box = mapping::localBox(shape);
    info_->setText(tr("%1 LEDs · Größe %2 × %3 × %4 · %5 Kanäle (RGB), rund %6 Universe(s)")
                       .arg(leds)
                       .arg(number(static_cast<double>(box.max.x - box.min.x)), number(static_cast<double>(box.max.y - box.min.y)),
                            number(static_cast<double>(box.max.z - box.min.z)))
                       .arg(leds * 3U)
                       .arg((leds + 169U) / 170U));
    autoInfo_->setText(tr("Universe %1 ab Kanal 1").arg(player_.suggestPatch(leds).universe));
}

QString AddFixtureDialog::exercise() {
    // Matrix: zwei Felder und die Startecke ändern.
    kind_->setCurrentIndex(0);
    columns_->setValue(5);
    rows_->setValue(3);
    corner_->setCurrentIndex(static_cast<int>(mapping::StartCorner::BottomRight));
    matrixSerpentine_->setChecked(false);
    NewFixture r = result();
    const auto* matrix = std::get_if<mapping::MatrixShape>(&r.shape);
    if (matrix == nullptr || matrix->columns != 5 || matrix->rows != 3 || matrix->wiring.start != mapping::StartCorner::BottomRight ||
        matrix->wiring.serpentine) {
        return QStringLiteral("Hinzufügen-Dialog: die Matrix-Felder kommen nicht im Ergebnis an");
    }
    if (!info_->text().contains(QStringLiteral("15 LEDs"))) {
        return QStringLiteral("Hinzufügen-Dialog: die Anzeige nennt nicht 15 LEDs (") + info_->text() + QLatin1Char(')');
    }
    // Strip
    kind_->setCurrentIndex(1);
    count_->setValue(30);
    end_.y->setValue(10.0);
    r = result();
    const auto* strip = std::get_if<mapping::StripShape>(&r.shape);
    if (strip == nullptr || strip->count != 30 || strip->end.y != 10.0F) {
        return QStringLiteral("Hinzufügen-Dialog: die Strip-Felder kommen nicht im Ergebnis an");
    }
    // Gitter mit allem: Anzahl, Abstand, Zählrichtung, Serpentine, Startecke, Position.
    kind_->setCurrentIndex(2);
    nx_->setValue(4);
    ny_->setValue(3);
    nz_->setValue(2);
    gridPitch_.x->setValue(2.0);
    gridPitch_.y->setValue(2.0);
    gridPitch_.z->setValue(2.0);
    order_->setCurrentIndex(static_cast<int>(mapping::AxisOrder::ZYX));
    gridSerpentine_->setChecked(true);
    flipX_->setChecked(true);
    flipY_->setChecked(false);
    flipZ_->setChecked(true);
    origin_.x->setValue(10.0);
    r = result();
    const auto* grid = std::get_if<mapping::GridShape>(&r.shape);
    if (grid == nullptr || grid->nx != 4 || grid->ny != 3 || grid->nz != 2 || grid->pitch.x != 2.0F ||
        grid->wiring.order != mapping::AxisOrder::ZYX || !grid->wiring.serpentine || !grid->wiring.flipX ||
        grid->wiring.flipY || !grid->wiring.flipZ || r.origin.x != 10.0F) {
        return QStringLiteral("Hinzufügen-Dialog: die Gitter-Felder kommen nicht im Ergebnis an");
    }
    if (!info_->text().contains(QStringLiteral("24 LEDs")) || !info_->text().contains(QStringLiteral("72 Kanäle"))) {
        return QStringLiteral("Hinzufügen-Dialog: die Anzeige nennt nicht 24 LEDs und 72 Kanäle (") + info_->text() + QLatin1Char(')');
    }
    // Eine zu große Form sperrt "Hinzufügen" und sagt warum.
    nx_->setValue(1024);
    ny_->setValue(1024);
    nz_->setValue(1024);
    if (buttons_->button(QDialogButtonBox::Ok)->isEnabled() || !info_->text().startsWith(QStringLiteral("⚠"))) {
        return QStringLiteral("Hinzufügen-Dialog: ein zu großes Gitter wird nicht abgelehnt");
    }
    nx_->setValue(4);
    ny_->setValue(3);
    nz_->setValue(2);
    if (!buttons_->button(QDialogButtonBox::Ok)->isEnabled()) {
        return QStringLiteral("Hinzufügen-Dialog: nach dem Zurückstellen bleibt \"Hinzufügen\" gesperrt");
    }
    // Patch: manuell mit Vorschlag als Start, dann Universe ändern; "Nicht senden"; zurück auf automatisch.
    patchMode_->setCurrentIndex(1);
    universe_->setValue(7);
    channel_->setValue(100);
    r = result();
    if (r.patchMode != PatchMode::Manual || r.patch.universe != 7 || r.patch.channel != 100) {
        return QStringLiteral("Hinzufügen-Dialog: der manuelle Patch kommt nicht im Ergebnis an");
    }
    patchMode_->setCurrentIndex(2);
    if (result().patchMode != PatchMode::None) {
        return QStringLiteral("Hinzufügen-Dialog: \"Nicht senden\" kommt nicht im Ergebnis an");
    }
    patchMode_->setCurrentIndex(0);
    if (result().patchMode != PatchMode::Auto) {
        return QStringLiteral("Hinzufügen-Dialog: \"Automatisch\" kommt nicht im Ergebnis an");
    }
    return {};
}

}  // namespace whitehole::studio
