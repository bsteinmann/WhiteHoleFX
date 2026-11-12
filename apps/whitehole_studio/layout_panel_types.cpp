// layout_panel_types.cpp: Bauformen der Show im Reiter Layout: Liste oben und Exemplare daraus auf der Bühne anlegen.
//
// Warum eine eigene Datei: layout_panel.cpp und layout_panel_add.cpp sind an der 400-Zeilen-Grenze. Die Bauformen selbst
// entstehen im Reiter Fixtures; hier wird nur ein Exemplar mit eigener Lage, Drehung und Patch daraus gemacht.
#include "layout_panel.hpp"

#include <QListWidget>
#include <QSignalBlocker>

#include <algorithm>

namespace whitehole::studio {

using form::fromStd;

void LayoutPanel::fillTypeList() {
    const QString keep = selectedTypeName();
    const QSignalBlocker blocker{types_};
    types_->clear();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        const QString name = fromStd(type.name);
        auto* item = new QListWidgetItem{tr("%1 (%2 LEDs)").arg(name).arg(mapping::ledCount(type.shape))};
        item->setData(Qt::UserRole, name);
        types_->addItem(item);
        if (name == keep) {
            types_->setCurrentItem(item);
        }
    }
    if (types_->count() == 0) {
        auto* none = new QListWidgetItem{tr("Noch keine Bauform (Stage Config → Fixtures → Neu)")};
        none->setFlags(Qt::NoItemFlags);
        types_->addItem(none);
    } else if (types_->currentRow() < 0) {
        types_->setCurrentRow(0);
    }
}

QString LayoutPanel::selectedTypeName() const {
    const QListWidgetItem* item = types_->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString{};
}

bool LayoutPanel::addFromType(const QString& typeName) {
    if (typeName.isEmpty()) {
        return false;  // nichts gewählt (zum Beispiel der Platzhalter „Noch keine Bauform“)
    }
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
    fillTypeList();
    if (selectedTypeName().isEmpty()) {
        return QStringLiteral("Aus Bauform: die Liste der Bauformen ist leer");
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
