// node_editor_targets.cpp: Ziel eines Output-Nodes wählen (Rechtsklick), für mehrere Outputs.
//
// Warum eine eigene Datei: node_editor_menu.cpp soll beim Menü bleiben. Ein Output-Node bekommt hier ein Untermenü
// "Ziel zuweisen" mit allen Fixtures, Gruppen und Regionen zum Anhaken. Das Rechteck des Outputs wird daraus
// berechnet (Player::setOutputTargets); ohne Haken gilt wieder das freie Rechteck aus den Parametern.
#include <QAction>
#include <QMenu>

#include <algorithm>
#include <string>
#include <vector>

#include "node_editor.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

void NodeEditor::addTargetMenu(QMenu& menu, NodeId id) {
    const project::Project* project = player_.project();
    const graph::NodeInstance* node = project != nullptr ? project->graph.findNode(id) : nullptr;
    if (node == nullptr || node->typeId != "wh.output.canvas") {
        return;
    }
    const std::vector<std::string> current = node->targets;
    QMenu* sub = menu.addMenu(tr("Ziel zuweisen"));
    const auto addEntry = [this, sub, id, current](const std::string& kind, const std::string& name) {
        const std::string entry = kind + ":" + name;
        QAction* action = sub->addAction(fromStd(name));
        action->setCheckable(true);
        action->setChecked(std::find(current.begin(), current.end(), entry) != current.end());
        connect(action, &QAction::triggered, this, [this, id, current, entry](bool on) {
            std::vector<std::string> targets = current;
            if (on) {
                targets.push_back(entry);
            } else {
                std::erase(targets, entry);
            }
            if (const Result<void> r = player_.setOutputTargets(id, std::move(targets)); !r.ok()) {
                fail(r.error().message);
                return;
            }
            rebuild();
            Q_EMIT edited(true);
        });
    };
    sub->addSection(tr("Fixtures"));
    for (const mapping::Fixture& fixture : project->layout.fixtures()) {
        addEntry("fixture", fixture.name);
    }
    if (!project->groups.empty()) {
        sub->addSection(tr("Gruppen"));
        for (const project::FixtureGroup& group : project->groups) {
            addEntry("group", group.name);
        }
    }
    if (!project->regions.empty()) {
        sub->addSection(tr("Regionen"));
        for (const project::StageRegion& region : project->regions) {
            addEntry("region", region.name);
        }
    }
    sub->addSeparator();
    QAction* clear = sub->addAction(tr("Kein Ziel (freies Rechteck)"));
    clear->setEnabled(!current.empty());
    connect(clear, &QAction::triggered, this, [this, id]() {
        if (player_.setOutputTargets(id, {}).ok()) {
            rebuild();
            Q_EMIT edited(true);
        }
    });
}

}  // namespace whitehole::studio
