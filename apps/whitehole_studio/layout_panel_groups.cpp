// layout_panel_groups.cpp: Gruppen und Regionen anlegen und löschen (Ziele für mehrere Outputs).
//
// Warum eine eigene Datei: layout_panel.cpp ist schon groß. Gruppen fassen Fixtures unter einem Namen zusammen,
// Regionen sind benannte Rechtecke der Bühne; beide wählt man danach per Rechtsklick an einem Output-Node.
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "layout_panel.hpp"

namespace whitehole::studio {

using form::fromStd;

void LayoutPanel::fillGroupMenu(QMenu* menu) {
    menu->clear();
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    QAction* save = menu->addAction(tr("Gewählte Fixtures als Gruppe speichern …"));
    save->setEnabled(!selectedIds().empty());
    connect(save, &QAction::triggered, this, [this]() { saveGroup(); });
    QAction* region = menu->addAction(tr("Region anlegen …"));
    connect(region, &QAction::triggered, this, [this]() { addRegion(); });
    menu->addSeparator();
    QMenu* removeGroup = menu->addMenu(tr("Gruppe löschen"));
    removeGroup->setEnabled(!project->groups.empty());
    for (const project::FixtureGroup& group : project->groups) {
        connect(removeGroup->addAction(fromStd(group.name)), &QAction::triggered, this, [this, name = group.name]() {
            std::vector<project::FixtureGroup> groups = player_.project()->groups;
            std::erase_if(groups, [&name](const project::FixtureGroup& g) { return g.name == name; });
            if (const Result<void> r = player_.setGroups(std::move(groups)); !r.ok()) {
                Q_EMIT failed(fromStd(r.error().message));
            }
        });
    }
    QMenu* removeRegion = menu->addMenu(tr("Region löschen"));
    removeRegion->setEnabled(!project->regions.empty());
    for (const project::StageRegion& region : project->regions) {
        connect(removeRegion->addAction(fromStd(region.name)), &QAction::triggered, this, [this, name = region.name]() {
            std::vector<project::StageRegion> regions = player_.project()->regions;
            std::erase_if(regions, [&name](const project::StageRegion& g) { return g.name == name; });
            if (const Result<void> r = player_.setRegions(std::move(regions)); !r.ok()) {
                Q_EMIT failed(fromStd(r.error().message));
            }
        });
    }
}

void LayoutPanel::saveGroup() {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    const std::vector<mapping::FixtureId> ids = selectedIds();
    project::FixtureGroup group;
    for (const mapping::Fixture& fixture : project->layout.fixtures()) {
        if (std::find(ids.begin(), ids.end(), fixture.id) != ids.end()) {
            group.fixtures.push_back(fixture.name);
        }
    }
    if (group.fixtures.empty()) {
        Q_EMIT failed(tr("Wähle zuerst Fixtures in der Liste."));
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("Gruppe speichern"), tr("Name der Gruppe"), QLineEdit::Normal,
                                               QString{}, &accepted)
                             .trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }
    group.name = name.toUtf8().toStdString();
    std::vector<project::FixtureGroup> groups = project->groups;
    std::erase_if(groups, [&group](const project::FixtureGroup& g) { return g.name == group.name; });  // ersetzt
    groups.push_back(std::move(group));
    if (const Result<void> r = player_.setGroups(std::move(groups)); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
    }
}

namespace {

/// Kleine Bühnenansicht für den Region-Dialog: die LEDs als Punkte, die Region als Rechteck. So sieht man sofort,
/// was die Zahlen bedeuten. Koordinaten sind Anteile der Leinwand; oben zählt von oben.
class RegionPreview : public QWidget {
public:
    RegionPreview(const mapping::LedMap& map, QWidget* parent) : QWidget{parent} {
        setFixedSize(260, 150);
        const std::size_t step = std::max<std::size_t>(1, map.unit.size() / 2000);  // höchstens ~2000 Punkte zeichnen
        for (std::size_t i = 0; i < map.unit.size(); i += step) {
            points_.emplace_back(map.unit[i].x, 1.0F - map.unit[i].y);
        }
    }

    void setRegion(double left, double top, double width, double height) {
        region_ = QRectF{left, top, width, height};
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.fillRect(rect(), QColor{24, 25, 30});
        const QRectF stage = QRectF{rect()}.adjusted(6, 6, -6, -6);
        painter.setPen(QPen{QColor{70, 74, 84}, 1.0});
        painter.drawRect(stage);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{150, 200, 255});
        for (const auto& [x, y] : points_) {
            painter.drawEllipse(QPointF{stage.left() + x * stage.width(), stage.top() + y * stage.height()}, 1.6, 1.6);
        }
        const QRectF box{stage.left() + region_.left() * stage.width(), stage.top() + region_.top() * stage.height(),
                         region_.width() * stage.width(), region_.height() * stage.height()};
        painter.setPen(QPen{QColor{255, 220, 0}, 2.0});
        painter.setBrush(QColor{255, 220, 0, 40});
        painter.drawRect(box);
    }

private:
    std::vector<std::pair<float, float>> points_;
    QRectF region_{0.0, 0.0, 0.5, 1.0};
};

}  // namespace

void LayoutPanel::addRegion() {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Region anlegen"));
    auto* layout = new QFormLayout{&dialog};
    auto* hint = new QLabel{tr("Eine Region ist ein Rechteck der Bühne. Ein Output mit dieser Region als Ziel bespielt "
                               "alle LEDs darin. Die Angaben sind Prozent der ganzen Bühne."),
                            &dialog};
    hint->setWordWrap(true);
    layout->addRow(hint);
    auto* name = new QLineEdit{tr("Region %1").arg(project->regions.size() + 1)};
    layout->addRow(tr("Name der Region"), name);
    const auto box = [&dialog](int value) {
        auto* spin = new QSpinBox{&dialog};
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral(" %"));
        spin->setValue(value);
        return spin;
    };
    QSpinBox* left = box(0);
    QSpinBox* top = box(0);
    QSpinBox* width = box(50);
    QSpinBox* height = box(100);
    auto* preview = new RegionPreview{player_.ledMap(), &dialog};
    const auto refresh = [=]() {
        preview->setRegion(left->value() / 100.0, top->value() / 100.0, width->value() / 100.0, height->value() / 100.0);
    };
    // Schnellwahl für die häufigen Fälle.
    auto* quick = new QWidget{&dialog};
    auto* quickRow = new QHBoxLayout{quick};
    quickRow->setContentsMargins(0, 0, 0, 0);
    const auto preset = [&](const QString& text, int l, int t, int w, int h) {
        auto* button = new QPushButton{text, quick};
        quickRow->addWidget(button);
        connect(button, &QPushButton::clicked, &dialog, [=]() {
            left->setValue(l);
            top->setValue(t);
            width->setValue(w);
            height->setValue(h);
        });
    };
    preset(tr("Links"), 0, 0, 50, 100);
    preset(tr("Rechts"), 50, 0, 50, 100);
    preset(tr("Oben"), 0, 0, 100, 50);
    preset(tr("Unten"), 0, 50, 100, 50);
    preset(tr("Mitte"), 25, 25, 50, 50);
    layout->addRow(tr("Schnellwahl"), quick);
    layout->addRow(tr("Links ab"), left);
    layout->addRow(tr("Oben ab"), top);
    layout->addRow(tr("Breite"), width);
    layout->addRow(tr("Höhe"), height);
    layout->addRow(tr("Vorschau"), preview);
    for (QSpinBox* spin : {left, top, width, height}) {
        connect(spin, &QSpinBox::valueChanged, &dialog, refresh);
    }
    refresh();
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) {
        return;
    }
    project::StageRegion region;
    region.name = name->text().trimmed().toUtf8().toStdString();
    region.left = static_cast<float>(left->value()) / 100.0F;
    region.top = static_cast<float>(top->value()) / 100.0F;
    region.width = static_cast<float>(std::min(width->value(), 100 - left->value())) / 100.0F;
    region.height = static_cast<float>(std::min(height->value(), 100 - top->value())) / 100.0F;
    std::vector<project::StageRegion> regions = project->regions;
    std::erase_if(regions, [&region](const project::StageRegion& r) { return r.name == region.name; });  // ersetzt
    regions.push_back(std::move(region));
    if (const Result<void> r = player_.setRegions(std::move(regions)); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
    }
}

}  // namespace whitehole::studio
