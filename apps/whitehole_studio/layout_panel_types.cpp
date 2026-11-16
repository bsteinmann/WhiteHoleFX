// layout_panel_types.cpp: Fixtures der Show im Reiter Layout: Liste oben und Exemplare daraus patchen.
//
// Warum eine eigene Datei: layout_panel.cpp und layout_panel_add.cpp sind an der 400-Zeilen-Grenze. Die Fixtures
// selbst entstehen im Reiter Fixtures; hier wird nur eine wählbare Anzahl Exemplare mit eigener Lage, Drehung und
// Patch daraus gemacht.
#include "layout_panel.hpp"

#include <QListWidget>
#include <QShowEvent>
#include <QSignalBlocker>

#include <algorithm>

#include "fixture_shape_label.hpp"

namespace whitehole::studio {

using form::fromStd;

void LayoutPanel::refreshTypes() {
    if (isVisible()) {
        fillTypeList();
    } else {
        typesDirty_ = true;
    }
}

void LayoutPanel::showEvent(QShowEvent* event) {
    QTabWidget::showEvent(event);
    if (typesDirty_) {
        typesDirty_ = false;
        fillTypeList();
    }
}

void LayoutPanel::fillTypeList() {
    const QString keep = selectedTypeName();
    const QSignalBlocker blocker{types_};
    types_->clear();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        const QString name = fromStd(type.name);
        // Name zuerst (frei vergeben), dahinter die Original-Bezeichnung der Form: sonst sieht man einem Namen wie
        // "Kachel 5x3" nicht mehr an, ob dahinter wirklich eine Matrix steckt (siehe fixture_shape_label.hpp).
        auto* item = new QListWidgetItem{
            tr("%1 — %2 (%3 LEDs)").arg(name, shapeKindLabel(type.shape)).arg(mapping::ledCount(type.shape))};
        item->setData(Qt::UserRole, name);
        types_->addItem(item);
        if (name == keep) {
            types_->setCurrentItem(item);
        }
    }
    if (types_->count() == 0) {
        auto* none = new QListWidgetItem{tr("Noch kein Fixture (Stage Config → Fixtures → Neu)")};
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

bool LayoutPanel::addFromType(const QString& typeName, int count) {
    if (typeName.isEmpty()) {
        return false;  // nichts gewählt (zum Beispiel der Platzhalter „Noch kein Fixture“)
    }
    if (player_.project() == nullptr) {
        Q_EMIT failed(tr("Es ist kein Projekt geladen. Mit Strg+N ein neues anlegen oder mit Strg+O eins öffnen."));
        return false;
    }
    const std::string type = typeName.toUtf8().toStdString();
    const auto found = std::ranges::find(player_.fixtureTypes(), type, &project::FixtureType::name);
    if (found == player_.fixtureTypes().end()) {
        Q_EMIT failed(tr("Es gibt kein Fixture \"%1\"").arg(typeName));
        return false;
    }
    const std::size_t ledCount = mapping::ledCount(found->shape);
    bool any = false;
    mapping::FixtureId last{};
    for (int i = 0; i < std::max(count, 1); ++i) {
        const QString name = uniqueName(typeName);
        const auto id = player_.addFixtureOfType(name.toUtf8().toStdString(), type, nextOrigin(), Vec3{},
                                                  player_.suggestPatch(ledCount));
        if (!id.ok()) {
            Q_EMIT failed(fromStd(id.error().message));
            break;
        }
        any = true;
        last = id.value();
    }
    if (!any) {
        return false;
    }
    fillList(last);
    Q_EMIT edited();
    return true;
}

QString LayoutPanel::exerciseTypes() {
    const std::size_t before = player_.project()->layout.fixtures().size();
    mapping::RingShape ring;
    ring.count = 12;
    if (!player_.addFixtureType("Rauchtest-Kreis", ring).ok()) {
        return QStringLiteral("Aus Fixture: Fixture lässt sich nicht anlegen");
    }
    if (!addFromType(QStringLiteral("Rauchtest-Kreis")) || player_.project()->layout.fixtures().size() != before + 1) {
        return QStringLiteral("Aus Fixture: Exemplar wurde nicht angelegt");
    }
    const mapping::Fixture& added = player_.project()->layout.fixtures().back();
    if (added.type != "Rauchtest-Kreis" || std::get_if<mapping::RingShape>(&added.shape) == nullptr) {
        return QStringLiteral("Aus Fixture: Exemplar hat nicht die Form des Fixtures");
    }
    fillTypeList();
    if (selectedTypeName().isEmpty()) {
        return QStringLiteral("Aus Fixture: die Fixture-Liste ist leer");
    }
    setSelectedIds({added.id});
    if (const QString note = fixture_->exerciseTyped(); !note.isEmpty()) {
        return note;
    }
    removeSelected();
    // Anzahl größer als 1: patcht mehrere Exemplare in einem Schritt.
    const std::size_t afterOne = player_.project()->layout.fixtures().size();
    if (!addFromType(QStringLiteral("Rauchtest-Kreis"), 3) ||
        player_.project()->layout.fixtures().size() != afterOne + 3) {
        return QStringLiteral("Aus Fixture: Anzahl > 1 legt nicht alle Exemplare an");
    }
    while (player_.project()->layout.fixtures().size() > before) {
        setSelectedIds({player_.project()->layout.fixtures().back().id});
        removeSelected();
    }
    if (!player_.removeFixtureType("Rauchtest-Kreis").ok() || player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Aus Fixture: Aufräumen scheitert");
    }
    return {};
}

}  // namespace whitehole::studio
