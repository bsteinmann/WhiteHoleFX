// layout_panel_grid.cpp: Raster und Einrasten im Layout-Editor (Bedienelemente und gemerkte Werte).
//
// Warum eine eigene Datei: layout_panel.cpp ist sonst über der 400-Zeilen-Grenze. Hier stehen die Schalter
// "Raster zeigen" und "Einrasten" und die Rasterweite; die Rechnung selbst liegt in wh_preview (grid.hpp),
// gezeichnet und angewendet wird sie vom PreviewController. Die Werte gelten für das Studio, nicht je
// Projekt, und werden zwischen Sitzungen gemerkt.
#include "layout_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QSettings>
#include <QSignalBlocker>

#include <cmath>

namespace whitehole::studio {

namespace {

constexpr double kSteps[] = {0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0};

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

QWidget* LayoutPanel::buildGridBar() {
    QSettings saved = settings();
    auto* bar = new QWidget;
    auto* row = new QHBoxLayout{bar};
    row->setContentsMargins(0, 0, 0, 0);

    showGrid_ = new QCheckBox{tr("Raster")};
    showGrid_->setToolTip(tr("Raster in der Vorschau anzeigen"));
    showGrid_->setChecked(saved.value(QStringLiteral("layout/gridShow"), true).toBool());
    snap_ = new QCheckBox{tr("Einrasten")};
    snap_->setToolTip(tr("Beim Ziehen rastet die linke untere Ecke der Fixtures am Raster ein (Alt gedrückt: ohne Einrasten)"));
    snap_->setChecked(saved.value(QStringLiteral("layout/gridSnap"), true).toBool());
    snapFixtures_ = new QCheckBox{tr("An Fixtures")};
    snapFixtures_->setToolTip(tr("Beim Ziehen rasten Kanten und Mitte an den Kanten und Mitten anderer Fixtures ein; Hilfslinien zeigen, "
                                 "woran (nur mit „Einrasten“; Alt gedrückt: ohne Einrasten)"));
    snapFixtures_->setChecked(saved.value(QStringLiteral("layout/fixtureSnap"), true).toBool());
    gridStep_ = new QComboBox;
    gridStep_->setToolTip(tr("Rasterweite in Layout-Einheiten"));
    const double savedStep = saved.value(QStringLiteral("layout/gridStep"), 1.0).toDouble();
    for (const double step : kSteps) {
        gridStep_->addItem(QLocale().toString(step), step);
        if (std::abs(step - savedStep) < 1e-9) {
            gridStep_->setCurrentIndex(gridStep_->count() - 1);
        }
    }
    if (gridStep_->currentIndex() < 0) {
        gridStep_->setCurrentIndex(3);
    }

    row->addWidget(showGrid_);
    row->addWidget(snap_);
    row->addWidget(snapFixtures_);
    row->addWidget(gridStep_);
    row->addStretch(1);

    const auto changed = [this]() {
        QSettings out = settings();
        out.setValue(QStringLiteral("layout/gridShow"), showGrid_->isChecked());
        out.setValue(QStringLiteral("layout/gridSnap"), snap_->isChecked());
        out.setValue(QStringLiteral("layout/fixtureSnap"), snapFixtures_->isChecked());
        out.setValue(QStringLiteral("layout/gridStep"), gridStep());
        Q_EMIT gridChanged();
    };
    connect(showGrid_, &QCheckBox::toggled, this, changed);
    connect(snap_, &QCheckBox::toggled, this, changed);
    connect(snapFixtures_, &QCheckBox::toggled, this, changed);
    connect(gridStep_, &QComboBox::currentIndexChanged, this, changed);
    return bar;
}

bool LayoutPanel::gridVisible() const { return showGrid_ != nullptr && showGrid_->isChecked(); }

bool LayoutPanel::snapEnabled() const { return snap_ != nullptr && snap_->isChecked(); }

bool LayoutPanel::fixtureSnapEnabled() const { return snapFixtures_ != nullptr && snapFixtures_->isChecked(); }

void LayoutPanel::setFixtureSnap(bool on) {
    const QSignalBlocker blocker{snapFixtures_};
    snapFixtures_->setChecked(on);
    Q_EMIT gridChanged();
}

float LayoutPanel::gridStep() const {
    return gridStep_ != nullptr ? static_cast<float>(gridStep_->currentData().toDouble()) : 1.0F;
}

void LayoutPanel::setGrid(bool show, bool snap, float step) {
    const QSignalBlocker a{showGrid_};
    const QSignalBlocker b{snap_};
    const QSignalBlocker c{gridStep_};
    showGrid_->setChecked(show);
    snap_->setChecked(snap);
    const int index = gridStep_->findData(static_cast<double>(step));
    if (index >= 0) {
        gridStep_->setCurrentIndex(index);
    }
    Q_EMIT gridChanged();
}

}  // namespace whitehole::studio
