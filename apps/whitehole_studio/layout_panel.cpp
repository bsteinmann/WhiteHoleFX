// layout_panel.cpp: Fixture-Liste und Seitenaufbau.
#include "layout_panel.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

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
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    pageLayout->addWidget(list_);

    auto* buttons = new QHBoxLayout;
    auto* addButton = new QPushButton{tr("Hinzufügen …")};
    addButton->setToolTip(tr("Matrix, Strip oder 3D-Gitter mit Größe, Lage, Verkabelung und Patch anlegen"));
    auto* importButton = new QPushButton{tr("Import …")};
    importButton->setToolTip(tr("LED-Positionen aus einer CSV- oder JSON-Datei als Punktwolke laden"));
    auto* removeButton = new QPushButton{tr("Entfernen")};
    buttons->addWidget(addButton);
    buttons->addWidget(importButton);
    buttons->addWidget(removeButton);
    pageLayout->addLayout(buttons);
    connect(addButton, &QPushButton::clicked, this, [this]() { addFromDialog(); });
    connect(importButton, &QPushButton::clicked, this, [this]() { importPoints(); });
    connect(removeButton, &QPushButton::clicked, this, [this]() { removeSelected(); });

    pageLayout->addWidget(buildArrangeBar());

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
    connect(list_, &QListWidget::itemSelectionChanged, this, [this]() { Q_EMIT selectionChanged(); });
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

std::vector<mapping::FixtureId> LayoutPanel::selectedIds() const {
    std::vector<mapping::FixtureId> ids;
    for (int i = 0; i < list_->count(); ++i) {
        const QListWidgetItem* item = list_->item(i);
        if (item->isSelected()) {
            ids.push_back(mapping::FixtureId{item->data(kIdRole).toULongLong()});
        }
    }
    return ids;
}

void LayoutPanel::setSelectedIds(const std::vector<mapping::FixtureId>& ids) {
    {
        const QSignalBlocker blocker{list_};
        list_->clearSelection();
        QListWidgetItem* first = nullptr;
        for (int i = 0; i < list_->count(); ++i) {
            QListWidgetItem* item = list_->item(i);
            const mapping::FixtureId id{item->data(kIdRole).toULongLong()};
            if (std::ranges::find(ids, id) != ids.end()) {
                item->setSelected(true);
                if (first == nullptr) {
                    first = item;
                }
            }
        }
        if (first != nullptr) {
            list_->setCurrentItem(first, QItemSelectionModel::NoUpdate);
        }
    }
    fixture_->showFixture(selectedId());
}

void LayoutPanel::refreshValues() {
    refreshTexts();
    fixture_->showFixture(selectedId());
}

QWidget* LayoutPanel::buildArrangeBar() {
    auto* bar = new QWidget;
    auto* grid = new QGridLayout{bar};
    grid->setContentsMargins(0, 0, 0, 0);
    struct Entry {
        QString text;
        QString tip;
        std::function<void()> action;
    };
    const std::vector<Entry> entries{
        {tr("⇤ Links"), tr("Linke Kanten ausrichten"), [this]() { align(runtime::Player::Align::Left); }},
        {tr("↔ Mitte"), tr("Waagrecht mittig ausrichten"), [this]() { align(runtime::Player::Align::HCenter); }},
        {tr("Rechts ⇥"), tr("Rechte Kanten ausrichten"), [this]() { align(runtime::Player::Align::Right); }},
        {tr("⤒ Oben"), tr("Obere Kanten ausrichten"), [this]() { align(runtime::Player::Align::Top); }},
        {tr("↕ Mitte"), tr("Senkrecht mittig ausrichten"), [this]() { align(runtime::Player::Align::VCenter); }},
        {tr("Unten ⤓"), tr("Untere Kanten ausrichten"), [this]() { align(runtime::Player::Align::Bottom); }},
        {tr("Verteilen ↔"), tr("Gleiche Lücken waagrecht (ab drei Fixtures)"),
         [this]() { distribute(runtime::Player::Axis::X); }},
        {tr("Verteilen ↕"), tr("Gleiche Lücken senkrecht (ab drei Fixtures)"),
         [this]() { distribute(runtime::Player::Axis::Y); }},
    };
    int index = 0;
    for (const Entry& entry : entries) {
        auto* button = new QPushButton{entry.text};
        button->setToolTip(entry.tip);
        connect(button, &QPushButton::clicked, this, entry.action);
        grid->addWidget(button, index / 3, index % 3);
        ++index;
    }
    return bar;
}

void LayoutPanel::align(runtime::Player::Align mode) {
    const std::vector<mapping::FixtureId> ids = selectedIds();
    if (player_.project() == nullptr || ids.empty()) {
        Q_EMIT failed(tr("Erst Fixtures wählen (Strg oder Umschalt halten für mehrere)"));
        return;
    }
    player_.closeEditGroup();
    const Result<void> r = player_.alignFixtures(ids, mode);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return;
    }
    refreshValues();
    Q_EMIT edited();
}

void LayoutPanel::distribute(runtime::Player::Axis axis) {
    const std::vector<mapping::FixtureId> ids = selectedIds();
    if (player_.project() == nullptr || ids.size() < 3) {
        Q_EMIT failed(tr("Zum Verteilen mindestens drei Fixtures wählen"));
        return;
    }
    player_.closeEditGroup();
    const Result<void> r = player_.distributeFixtures(ids, axis);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return;
    }
    refreshValues();
    Q_EMIT edited();
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

void LayoutPanel::removeSelected() {
    std::vector<mapping::FixtureId> ids = selectedIds();
    if (ids.empty() && list_->currentItem() != nullptr) {
        ids.push_back(selectedId());
    }
    if (player_.project() == nullptr || ids.empty()) {
        return;
    }
    // Jedes gewählte Fixture, nicht nur das zuletzt angelegte; beim ersten Fehler (z. B. das letzte
    // gepatchte Fixture) wird abgebrochen und der Grund gemeldet.
    for (const mapping::FixtureId id : ids) {
        const Result<void> r = player_.removeFixture(id);
        if (!r.ok()) {
            Q_EMIT failed(fromStd(r.error().message));
            break;
        }
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
    if (const QString note = exerciseImport(); !note.isEmpty()) {
        return note;
    }
    if (const QString note = exerciseAddDialog(); !note.isEmpty()) {
        return note;
    }
    return exerciseArrange();
}

QString LayoutPanel::exerciseArrange() {
    const std::size_t before = player_.project()->layout.fixtures().size();
    addStrip();
    addStrip();
    std::vector<mapping::FixtureId> all;
    for (const mapping::Fixture& fixture : player_.project()->layout.fixtures()) {
        all.push_back(fixture.id);
    }
    if (all.size() != before + 2) {
        return QStringLiteral("Anordnen: zwei Strips wurden nicht angelegt");
    }
    setSelectedIds(all);
    if (selectedIds().size() != all.size()) {
        return QStringLiteral("Anordnen: Mehrfachauswahl hat %1 statt %2 Fixtures").arg(selectedIds().size()).arg(all.size());
    }
    align(runtime::Player::Align::Bottom);
    const auto& fixtures = player_.project()->layout.fixtures();
    for (const mapping::Fixture& fixture : fixtures) {
        const float bottom = fixture.origin.y + mapping::localBox(fixture.shape).min.y;
        const mapping::Fixture& first = fixtures.front();
        if (std::abs(bottom - (first.origin.y + mapping::localBox(first.shape).min.y)) > 1e-3F) {
            return QStringLiteral("Anordnen: untere Kanten liegen nach \"Unten\" nicht gleich");
        }
    }
    distribute(runtime::Player::Axis::X);
    // Ein Fixture vorn in der Liste entfernen (nicht nur das zuletzt angelegte), dann zurücknehmen.
    const std::size_t withAdded = player_.project()->layout.fixtures().size();
    setSelectedIds({player_.project()->layout.fixtures().front().id});
    removeSelected();
    if (player_.project()->layout.fixtures().size() != withAdded - 1) {
        return QStringLiteral("Entfernen: das erste Fixture liess sich nicht entfernen (%1 statt %2 Fixtures)")
            .arg(player_.project()->layout.fixtures().size())
            .arg(withAdded - 1);
    }
    if (!player_.undo().ok()) {
        return QStringLiteral("Entfernen: Rückgängig scheiterte");
    }
    rebuild();
    // Aufräumen: die zwei Strips wieder entfernen (je einzeln gewählt, das Formular folgt der Auswahl).
    for (int i = 0; i < 2; ++i) {
        setSelectedIds({player_.project()->layout.fixtures().back().id});
        removeSelected();
    }
    if (player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Anordnen: Aufräumen hat %1 statt %2 Fixtures hinterlassen")
            .arg(player_.project()->layout.fixtures().size())
            .arg(before);
    }
    return {};
}

}  // namespace whitehole::studio
