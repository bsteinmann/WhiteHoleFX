// layout_panel_types.cpp: Exemplare einer Bauform aus der Bibliothek auf der Bühne anlegen (Knopf "Aus Bauform").
//
// Warum eine eigene Datei: layout_panel.cpp und layout_panel_add.cpp sind an der 400-Zeilen-Grenze. Die Bauformen selbst
// entstehen im Reiter Fixtures; hier wird nur ein Exemplar mit eigener Lage, Drehung und Patch daraus gemacht.
#include "layout_panel.hpp"

#include <QAction>
#include <QMenu>

#include <algorithm>

namespace whitehole::studio {

using form::fromStd;

void LayoutPanel::fillTypeMenu(QMenu* menu) {
    menu->clear();
    if (player_.fixtureTypes().empty()) {
        menu->addAction(tr("Noch keine Bauform (Stage Config → Fixtures)"))->setEnabled(false);
        return;
    }
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        const QString name = fromStd(type.name);
        QAction* action = menu->addAction(tr("%1 (%2 LEDs)").arg(name).arg(mapping::ledCount(type.shape)));
        connect(action, &QAction::triggered, this, [this, name]() { addFromType(name); });
    }
}

bool LayoutPanel::addFromType(const QString& typeName) {
    if (player_.project() == nullptr) {
        Q_EMIT failed(tr("Es ist kein Projekt geladen. Mit Strg+N ein neues anlegen oder mit Strg+O eins öffnen."));
        return false;
    }
    const std::string type = typeName.toUtf8().toStdString();
    const auto found = std::ranges::find(player_.fixtureTypes(), type, &project::FixtureType::name);
    if (found == player_.fixtureTypes().end()) {
        Q_EMIT failed(tr("Es gibt keine Bauform \"%1\"").arg(typeName));
        return false;
    }
    const QString name = uniqueName(typeName);
    const auto id = player_.addFixtureOfType(name.toUtf8().toStdString(), type, nextOrigin(), Vec3{},
                                             player_.suggestPatch(mapping::ledCount(found->shape)));
    if (!id.ok()) {
        Q_EMIT failed(fromStd(id.error().message));
        return false;
    }
    fillList(id.value());
    Q_EMIT edited();
    return true;
}

QString LayoutPanel::exerciseTypes() {
    const std::size_t before = player_.project()->layout.fixtures().size();
    mapping::RingShape ring;
    ring.count = 12;
    if (!player_.addFixtureType("Rauchtest-Kreis", ring).ok()) {
        return QStringLiteral("Aus Bauform: Bauform lässt sich nicht anlegen");
    }
    if (!addFromType(QStringLiteral("Rauchtest-Kreis")) || player_.project()->layout.fixtures().size() != before + 1) {
        return QStringLiteral("Aus Bauform: Exemplar wurde nicht angelegt");
    }
    const mapping::Fixture& added = player_.project()->layout.fixtures().back();
    if (added.type != "Rauchtest-Kreis" || std::get_if<mapping::RingShape>(&added.shape) == nullptr) {
        return QStringLiteral("Aus Bauform: Exemplar hat nicht die Form der Bauform");
    }
    QMenu menu;
    fillTypeMenu(&menu);
    if (menu.actions().isEmpty()) {
        return QStringLiteral("Aus Bauform: das Menü ist leer");
    }
    setSelectedIds({added.id});
    if (const QString note = fixture_->exerciseTyped(); !note.isEmpty()) {
        return note;
    }
    removeSelected();
    if (!player_.removeFixtureType("Rauchtest-Kreis").ok() || player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Aus Bauform: Aufräumen scheitert");
    }
    return {};
}

}  // namespace whitehole::studio
