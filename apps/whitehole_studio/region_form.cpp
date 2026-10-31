// region_form.cpp: Formular für den Effektbereich, siehe region_form.hpp.
#include "region_form.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "form_helpers.hpp"

namespace whitehole::studio {

using form::fromStd;

RegionForm::RegionForm(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* outer = new QVBoxLayout{this};
    auto* hint = new QLabel{tr("Der Effekt füllt nur dieses Rechteck der Wand (Bild-in-Bild). "
                               "Alle LEDs außerhalb bleiben aus.")};
    hint->setWordWrap(true);
    outer->addWidget(hint);

    enabled_ = new QCheckBox{tr("Effektbereich verwenden")};
    outer->addWidget(enabled_);

    auto* fields = new QFormLayout;
    x_ = form::doubleBox(0.0, 0.0, 100.0, 1.0, 1);
    y_ = form::doubleBox(0.0, 0.0, 100.0, 1.0, 1);
    w_ = form::doubleBox(100.0, 0.1, 100.0, 1.0, 1);
    h_ = form::doubleBox(100.0, 0.1, 100.0, 1.0, 1);
    for (QDoubleSpinBox* box : {x_, y_, w_, h_}) {
        box->setSuffix(QStringLiteral(" %"));
    }
    fields->addRow(tr("Links (x)"), x_);
    fields->addRow(tr("Oben (y)"), y_);
    fields->addRow(tr("Breite"), w_);
    fields->addRow(tr("Höhe"), h_);
    outer->addLayout(fields);

    draw_ = new QPushButton{tr("In der Vorschau zeichnen")};
    draw_->setCheckable(true);
    draw_->setToolTip(tr("Rahmen im Vorschaubild aufziehen, verschieben oder an den Ecken ändern"));
    outer->addWidget(draw_);
    outer->addStretch();

    connect(enabled_, &QCheckBox::toggled, this, [this](bool on) { applyEnabled(on); });
    for (QDoubleSpinBox* box : {x_, y_, w_, h_}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { applyValues(); });
    }
    connect(draw_, &QPushButton::toggled, this, [this](bool on) { Q_EMIT drawModeChanged(on); });
    rebuild();
}

void RegionForm::setDrawMode(bool on) {
    if (draw_->isChecked() != on) {
        draw_->setChecked(on);  // sendet drawModeChanged
    }
}

void RegionForm::rebuild() {
    const project::Project* project = player_.project();
    const bool has = project != nullptr && project->region.has_value();
    populating_ = true;
    {
        const QSignalBlocker a{enabled_};
        const QSignalBlocker b{x_};
        const QSignalBlocker c{y_};
        const QSignalBlocker d{w_};
        const QSignalBlocker e{h_};
        enabled_->setChecked(has);
        const mapping::Region r = has ? *project->region : mapping::Region{};
        x_->setValue(static_cast<double>(r.x) * 100.0);
        y_->setValue(static_cast<double>(r.y) * 100.0);
        w_->setValue(static_cast<double>(r.w) * 100.0);
        h_->setValue(static_cast<double>(r.h) * 100.0);
    }
    for (QWidget* widget : {static_cast<QWidget*>(x_), static_cast<QWidget*>(y_), static_cast<QWidget*>(w_),
                            static_cast<QWidget*>(h_)}) {
        widget->setEnabled(has);
    }
    enabled_->setEnabled(project != nullptr);
    draw_->setEnabled(project != nullptr);
    populating_ = false;
}

void RegionForm::applyEnabled(bool on) {
    if (populating_ || player_.project() == nullptr) {
        return;
    }
    const Result<void> r = player_.setRegion(on ? std::optional<mapping::Region>{mapping::Region{}} : std::nullopt);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
    }
    rebuild();
    if (!on) {
        setDrawMode(false);
    }
    Q_EMIT edited();
}

void RegionForm::applyValues() {
    if (populating_ || player_.project() == nullptr) {
        return;
    }
    mapping::Region region;
    region.x = static_cast<float>(x_->value() / 100.0);
    region.y = static_cast<float>(y_->value() / 100.0);
    region.w = static_cast<float>(w_->value() / 100.0);
    region.h = static_cast<float>(h_->value() / 100.0);
    const Result<void> r = player_.setRegion(region);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        rebuild();  // zurück auf den gültigen Stand
        return;
    }
    Q_EMIT edited();
}

QString RegionForm::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("Bereich: kein Projekt");
    }
    enabled_->setChecked(true);
    if (!player_.project()->region.has_value()) {
        return QStringLiteral("Bereich: Einschalten hat keinen Bereich angelegt");
    }
    w_->setValue(50.0);
    h_->setValue(50.0);
    if (!player_.project()->region || player_.project()->region->w != 0.5F) {
        return QStringLiteral("Bereich: Breite 50 % kam nicht im Projekt an");
    }
    x_->setValue(80.0);  // ragt hinaus (80 + 50 > 100): wird abgelehnt, die Anzeige springt zurück
    if (player_.project()->region->x != 0.0F || x_->value() != 0.0) {
        return QStringLiteral("Bereich: ungültiger Bereich wurde nicht abgelehnt");
    }
    enabled_->setChecked(false);
    if (player_.project()->region.has_value()) {
        return QStringLiteral("Bereich: Ausschalten hat den Bereich nicht entfernt");
    }
    return {};
}

}  // namespace whitehole::studio
