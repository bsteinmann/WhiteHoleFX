// layout_panel_groups.cpp: Gruppen und Regionen anlegen und löschen (Ziele für mehrere Outputs).
//
// Warum eine eigene Datei: layout_panel.cpp ist schon groß. Gruppen fassen Fixtures unter einem Namen zusammen,
// Regionen sind benannte Rechtecke der Bühne; beide wählt man danach per Rechtsklick an einem Output-Node.
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>

#include <algorithm>
#include <string>
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

void LayoutPanel::addRegion() {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Region anlegen"));
    auto* layout = new QFormLayout{&dialog};
    auto* name = new QLineEdit;
    layout->addRow(tr("Name der Region"), name);
    const auto box = [&dialog](double value) {
        auto* spin = new QDoubleSpinBox{&dialog};
        spin->setRange(0.0, 1.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.05);
        spin->setValue(value);
        return spin;
    };
    QDoubleSpinBox* left = box(0.0);
    QDoubleSpinBox* top = box(0.0);
    QDoubleSpinBox* width = box(0.5);
    QDoubleSpinBox* height = box(0.5);
    layout->addRow(tr("Links (Anteil der Leinwand)"), left);
    layout->addRow(tr("Oben (Anteil der Leinwand)"), top);
    layout->addRow(tr("Breite"), width);
    layout->addRow(tr("Höhe"), height);
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) {
        return;
    }
    project::StageRegion region;
    region.name = name->text().trimmed().toUtf8().toStdString();
    region.left = static_cast<float>(left->value());
    region.top = static_cast<float>(top->value());
    region.width = static_cast<float>(width->value());
    region.height = static_cast<float>(height->value());
    std::vector<project::StageRegion> regions = project->regions;
    std::erase_if(regions, [&region](const project::StageRegion& r) { return r.name == region.name; });  // ersetzt
    regions.push_back(std::move(region));
    if (const Result<void> r = player_.setRegions(std::move(regions)); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
    }
}

}  // namespace whitehole::studio
