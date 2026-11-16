// fixtures_page.cpp: Umsetzung zu fixtures_page.hpp.
#include "fixtures_page.hpp"

#include <QListWidget>
#include <QShowEvent>
#include <QVBoxLayout>

#include "form_helpers.hpp"

namespace whitehole::studio {

using form::fromStd;

FixturesPage::FixturesPage(runtime::Player& player, QWidget* parent)
    : QWidget{parent}, player_{player}, list_{new QListWidget{this}} {
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(list_);
    rebuild();
}

void FixturesPage::rebuild() {
    dirty_ = false;
    fillList();
}

void FixturesPage::refresh() {
    if (isVisible()) {
        rebuild();
    } else {
        dirty_ = true;
    }
}

void FixturesPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (dirty_) {
        rebuild();
    }
}

void FixturesPage::fillList() {
    list_->clear();
    if (player_.project() == nullptr) {
        auto* none = new QListWidgetItem{tr("Es ist noch kein Projekt geladen.")};
        none->setFlags(Qt::NoItemFlags);
        list_->addItem(none);
        return;
    }
    for (const mapping::Fixture& fixture : player_.project()->layout.fixtures()) {
        // Bauform (type) nennen, wenn das Fixture eine hat - sonst hat es eine eigene Form (bearbeitet nach wie
        // vor im Reiter Layout, nicht hier: diese Seite zeigt nur die Exemplare, siehe fixtures_page.hpp).
        const QString shapeInfo =
            fixture.type.empty() ? tr("eigene Form") : tr("Bauform „%1“").arg(fromStd(fixture.type));
        auto* item = new QListWidgetItem{
            tr("%1 — %2 (%3 LEDs)").arg(fromStd(fixture.name), shapeInfo).arg(mapping::ledCount(fixture.shape))};
        item->setData(Qt::UserRole, fixture.id.value());
        list_->addItem(item);
    }
    if (list_->count() == 0) {
        auto* none = new QListWidgetItem{
            tr("Noch kein Fixture auf der Bühne (Reiter Layout → Fixture-Bibliothek oder „Fixture hinzufügen …“)")};
        none->setFlags(Qt::NoItemFlags);
        list_->addItem(none);
    }
}

QString FixturesPage::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("Fixtures: kein Projekt");
    }
    rebuild();
    const int before = list_->count();
    mapping::StripShape shape;
    shape.count = 5;
    shape.end = Vec3{5.0F, 0.0F, 0.0F};
    const auto added = player_.addFixture("Rauchtest-Fixtures-Seite", Vec3{}, shape);
    if (!added.ok()) {
        return QStringLiteral("Fixtures: Fixture lässt sich nicht anlegen");
    }
    rebuild();
    bool found = false;
    for (int row = 0; row < list_->count(); ++row) {
        if (list_->item(row)->text().contains(QStringLiteral("Rauchtest-Fixtures-Seite"))) {
            found = true;
        }
    }
    if (!found || list_->count() != before + 1) {
        player_.removeFixture(added.value());
        return QStringLiteral("Fixtures: angelegtes Fixture erscheint nicht in der Liste");
    }
    if (!player_.removeFixture(added.value()).ok()) {
        return QStringLiteral("Fixtures: Aufräumen schlägt fehl");
    }
    rebuild();
    if (list_->count() != before) {
        return QStringLiteral("Fixtures: entferntes Fixture verschwindet nicht aus der Liste");
    }
    return {};
}

}  // namespace whitehole::studio
