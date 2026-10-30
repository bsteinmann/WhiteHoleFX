// layout_panel.cpp: Fixture-Liste und Seitenaufbau.
#include "layout_panel.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace whitehole::studio {

using form::fromStd;

namespace {

constexpr int kIdRole = Qt::UserRole;

}  // namespace

LayoutPanel::LayoutPanel(runtime::Player& player, QWidget* parent) : QTabWidget{parent}, player_{player} {
    auto* page = new QWidget;
    auto* pageLayout = new QVBoxLayout{page};

    list_ = new QListWidget;
    list_->setMaximumHeight(140);
    pageLayout->addWidget(list_);

    auto* buttons = new QHBoxLayout;
    auto* addMatrixButton = new QPushButton{tr("Matrix +")};
    auto* addStripButton = new QPushButton{tr("Strip +")};
    auto* removeButton = new QPushButton{tr("Entfernen")};
    buttons->addWidget(addMatrixButton);
    buttons->addWidget(addStripButton);
    buttons->addWidget(removeButton);
    pageLayout->addLayout(buttons);
    connect(addMatrixButton, &QPushButton::clicked, this, [this]() { addMatrix(); });
    connect(addStripButton, &QPushButton::clicked, this, [this]() { addStrip(); });
    connect(removeButton, &QPushButton::clicked, this, [this]() { removeSelected(); });

    fixture_ = new FixtureForm{player_};
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(fixture_);
    pageLayout->addWidget(scroll, 1);
    addTab(page, tr("Fixtures"));

    output_ = new OutputForm{player_};
    auto* outputScroll = new QScrollArea;
    outputScroll->setWidgetResizable(true);
    outputScroll->setWidget(output_);
    addTab(outputScroll, tr("Ausgabe"));

    connect(list_, &QListWidget::currentRowChanged, this, [this](int) { fixture_->showFixture(selectedId()); });
    connect(fixture_, &FixtureForm::edited, this, [this]() {
        refreshTexts();
        Q_EMIT edited();
    });
    connect(fixture_, &FixtureForm::failed, this, &LayoutPanel::failed);
    connect(output_, &OutputForm::edited, this, &LayoutPanel::edited);
    connect(output_, &OutputForm::failed, this, &LayoutPanel::failed);
}

mapping::FixtureId LayoutPanel::selectedId() const {
    const QListWidgetItem* item = list_->currentItem();
    return item != nullptr ? mapping::FixtureId{item->data(kIdRole).toULongLong()} : mapping::FixtureId{};
}

void LayoutPanel::rebuild() {
    fillList(selectedId());
    output_->rebuild();
}

void LayoutPanel::fillList(mapping::FixtureId select) {
    {
        const QSignalBlocker blocker{list_};
        list_->clear();
        if (const project::Project* project = player_.project()) {
            for (const mapping::Fixture& fixture : project->layout.fixtures()) {
                auto* item = new QListWidgetItem{QString{}};
                item->setData(kIdRole, static_cast<qulonglong>(fixture.id.value()));
                list_->addItem(item);
            }
        }
    }
    refreshTexts();
    int row = 0;
    for (int i = 0; i < list_->count(); ++i) {
        if (list_->item(i)->data(kIdRole).toULongLong() == select.value()) {
            row = i;
        }
    }
    {
        const QSignalBlocker blocker{list_};
        list_->setCurrentRow(list_->count() > 0 ? row : -1);
    }
    fixture_->showFixture(selectedId());
}

void LayoutPanel::refreshTexts() {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        const mapping::Fixture* fixture = project->layout.find(mapping::FixtureId{item->data(kIdRole).toULongLong()});
        if (fixture != nullptr) {
            item->setText(tr("%1  (%2 LEDs)").arg(fromStd(fixture->name)).arg(mapping::ledCount(fixture->shape)));
        }
    }
}

QString LayoutPanel::uniqueName(const QString& base) const {
    const project::Project* project = player_.project();
    for (int i = 1;; ++i) {
        const QString candidate = QStringLiteral("%1 %2").arg(base).arg(i);
        const std::string name = candidate.toUtf8().toStdString();
        const bool taken = project != nullptr &&
                           std::ranges::any_of(project->layout.fixtures(),
                                               [&](const mapping::Fixture& f) { return f.name == name; });
        if (!taken) {
            return candidate;
        }
    }
}

Vec3 LayoutPanel::nextOrigin() const {
    float right = 0.0F;
    bool any = false;
    if (const project::Project* project = player_.project()) {
        for (const mapping::Fixture& fixture : project->layout.fixtures()) {
            right = std::max(right, fixture.origin.x + mapping::localBox(fixture.shape).max.x);
            any = true;
        }
    }
    return Vec3{any ? right + 4.0F : 0.0F, 0.0F, 0.0F};
}

void LayoutPanel::addFixture(const QString& baseName, mapping::FixtureShape shape) {
    if (player_.project() == nullptr) {
        return;
    }
    const std::uint64_t leds = mapping::ledCount(shape);
    const auto id = player_.addFixture(uniqueName(baseName).toUtf8().toStdString(), nextOrigin(), std::move(shape),
                                       player_.suggestPatch(leds));
    if (!id.ok()) {
        Q_EMIT failed(fromStd(id.error().message));
        return;
    }
    fillList(id.value());
    Q_EMIT edited();
}

void LayoutPanel::addMatrix() {
    mapping::MatrixShape m;
    m.columns = 16;
    m.rows = 8;
    m.wiring.serpentine = true;
    addFixture(tr("Matrix"), m);
}

void LayoutPanel::addStrip() {
    mapping::StripShape s;
    s.count = 60;
    s.end = Vec3{60.0F, 0.0F, 0.0F};
    addFixture(tr("Strip"), s);
}

void LayoutPanel::removeSelected() {
    const mapping::FixtureId id = selectedId();
    if (player_.project() == nullptr || list_->currentItem() == nullptr) {
        return;
    }
    const Result<void> r = player_.removeFixture(id);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return;
    }
    fillList(mapping::FixtureId{});
    Q_EMIT edited();
}

QString LayoutPanel::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("Layout: kein Projekt");
    }
    const std::size_t fixtures = player_.project()->layout.fixtures().size();
    addStrip();
    if (player_.project()->layout.fixtures().size() != fixtures + 1 ||
        list_->count() != static_cast<int>(fixtures + 1)) {
        return QStringLiteral("Layout: Strip wurde nicht angelegt (%1 Fixtures, %2 Einträge)")
            .arg(player_.project()->layout.fixtures().size())
            .arg(list_->count());
    }
    if (const QString note = fixture_->exercise(); !note.isEmpty()) {
        return note;
    }
    if (const QString note = output_->exercise(); !note.isEmpty()) {
        return note;
    }
    removeSelected();
    if (player_.project()->layout.fixtures().size() != fixtures || list_->count() != static_cast<int>(fixtures)) {
        return QStringLiteral("Layout: Strip wurde nicht entfernt");
    }
    return {};
}

}  // namespace whitehole::studio
