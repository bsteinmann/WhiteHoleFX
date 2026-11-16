// fixture_library_page.cpp: Umsetzung zu fixture_library_page.hpp.
#include "fixture_library_page.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "fixture_catalog.hpp"
#include "form_helpers.hpp"
#include "shape_form.hpp"
#include "shape_preview.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

struct Preset {
    QString name;
    mapping::FixtureShape shape;
};

Preset presetOf(FixtureLibraryPage::Kind kind) {
    using Kind = FixtureLibraryPage::Kind;
    switch (kind) {
        case Kind::Matrix3d: {
            mapping::GridShape g;
            g.nx = g.ny = g.nz = 4;
            return {QObject::tr("3D-Matrix 4x4x4"), g};
        }
        case Kind::Circle: {
            mapping::RingShape r;
            r.count = 24;
            r.radius = 4.0F;
            return {QObject::tr("Kreis 24"), r};
        }
        case Kind::Strip: {
            mapping::StripShape s;
            s.count = 30;
            s.end = Vec3{30.0F, 0.0F, 0.0F};
            return {QObject::tr("Strip 30"), s};
        }
        case Kind::Column: {
            mapping::HelixShape h;
            h.count = 60;
            h.radius = 3.0F;
            h.height = 20.0F;
            h.turns = 3.0F;
            return {QObject::tr("Spirale 60"), h};
        }
        case Kind::Cylinder: {
            mapping::CylinderShape c;  // 12 LEDs je Ring, 20 Ringe
            return {QObject::tr("Säule 12x20"), c};
        }
        case Kind::Matrix2d:
        default: {
            mapping::MatrixShape m;
            m.columns = 8;
            m.rows = 8;
            return {QObject::tr("2D-Matrix 8x8"), m};
        }
    }
}

}  // namespace

FixtureLibraryPage::FixtureLibraryPage(runtime::Player& player, QWidget* parent)
    : QWidget{parent}, player_{player}, list_{new QListWidget{this}}, name_{new QLineEdit{this}},
      usage_{new QLabel{this}}, form_{new ShapeForm{this}}, preview_{new ShapePreview{this}},
      addButton_{new QPushButton{tr("Neu"), this}}, duplicate_{new QPushButton{tr("Duplizieren"), this}},
      remove_{new QPushButton{tr("Entfernen"), this}},
      saveToLibrary_{new QPushButton{tr("In Bibliothek speichern"), this}},
      deleteFromLibrary_{new QPushButton{tr("Aus Bibliothek löschen"), this}} {
    auto* split = new QSplitter{Qt::Horizontal, this};
    auto* outer = new QHBoxLayout{this};
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addWidget(split);

    auto* left = new QWidget;
    auto* leftColumn = new QVBoxLayout{left};
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->addWidget(new QLabel{tr("<b>Fixtures</b>")});
    leftColumn->addWidget(list_, 1);
    auto* menu = new QMenu{addButton_};
    const struct {
        const char* text;
        Kind kind;
    } kinds[] = {{QT_TR_NOOP("2D-Matrix"), Kind::Matrix2d}, {QT_TR_NOOP("3D-Matrix"), Kind::Matrix3d},
                 {QT_TR_NOOP("Kreis"), Kind::Circle},      {QT_TR_NOOP("Strip"), Kind::Strip},
                 {QT_TR_NOOP("Säule (Ringe)"), Kind::Cylinder}, {QT_TR_NOOP("Spirale (Säule)"), Kind::Column}};
    for (const auto& entry : kinds) {
        const Kind kind = entry.kind;
        QAction* action = menu->addAction(tr(entry.text));
        connect(action, &QAction::triggered, this, [this, kind]() { addType(kind); });
    }
    addButton_->setMenu(menu);
    addButton_->setToolTip(tr("Neues Fixture anlegen"));
    duplicate_->setToolTip(tr("Kopie des gewählten Fixtures anlegen"));
    remove_->setToolTip(tr("Fixture aus dieser Show entfernen. Schon platzierte Exemplare im Layout behalten ihre "
                            "Form als eigene."));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(addButton_);
    buttons->addWidget(duplicate_);
    buttons->addWidget(remove_);
    leftColumn->addLayout(buttons);
    saveToLibrary_->setToolTip(tr("Speichert das gewählte Fixture in der Bibliothek (Benutzerverzeichnis), damit du "
                                  "es in anderen Shows wiederfindest. Gespeichert in: %1").arg(userlib::path()));
    deleteFromLibrary_->setToolTip(tr("Nimmt das gewählte Fixture aus der Bibliothek; die Show bleibt unverändert"));
    auto* libraryButtons = new QHBoxLayout;
    libraryButtons->addWidget(saveToLibrary_);
    libraryButtons->addWidget(deleteFromLibrary_);
    leftColumn->addLayout(libraryButtons);
    split->addWidget(left);

    auto* middle = new QWidget;
    auto* middleColumn = new QVBoxLayout{middle};
    middleColumn->setContentsMargins(0, 0, 0, 0);
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel{tr("Name")});
    nameRow->addWidget(name_, 1);
    middleColumn->addLayout(nameRow);
    middleColumn->addWidget(usage_);
    middleColumn->addWidget(form_);
    middleColumn->addStretch(1);
    split->addWidget(middle);
    split->addWidget(preview_);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 0);
    split->setStretchFactor(2, 1);
    split->setSizes({230, 320, 400});

    connect(list_, &QListWidget::currentRowChanged, this, [this](int) { showSelected(); });
    connect(saveToLibrary_, &QPushButton::clicked, this, [this]() { saveSelectedToLibrary(); });
    connect(deleteFromLibrary_, &QPushButton::clicked, this, [this]() { removeFromLibrary(); });
    connect(duplicate_, &QPushButton::clicked, this, [this]() { duplicateSelected(); });
    connect(remove_, &QPushButton::clicked, this, [this]() { removeSelected(); });
    connect(name_, &QLineEdit::editingFinished, this, [this]() { renameSelected(name_->text()); });
    connect(form_, &ShapeForm::shapeEdited, this, [this](const mapping::FixtureShape& shape) { applyShape(shape); });
    rebuild();
}

QString FixtureLibraryPage::selectedName() const {
    const QListWidgetItem* item = list_->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString{};
}

void FixtureLibraryPage::selectType(const QString& name) {
    for (int row = 0; row < list_->count(); ++row) {
        if (list_->item(row)->data(Qt::UserRole).toString() == name) {
            list_->setCurrentRow(row);
            return;
        }
    }
}

void FixtureLibraryPage::rebuild() {
    if (editing_) {
        return;
    }
    dirty_ = false;
    fillList(selectedName());
}

void FixtureLibraryPage::refresh() {
    if (isVisible()) {
        rebuild();
    } else {
        dirty_ = true;
    }
}

void FixtureLibraryPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (dirty_) {
        rebuild();
    }
}

void FixtureLibraryPage::fillList(const QString& select) {
    const QSignalBlocker blocker{list_};
    list_->clear();
    for (const FixtureEntry& entry : mergedFixtures(player_)) {
        auto* item =
            new QListWidgetItem{tr("%1 (%2 LEDs)").arg(entry.name).arg(mapping::ledCount(entry.shape))};
        item->setData(Qt::UserRole, entry.name);
        item->setData(Qt::UserRole + 1, entry.inProject);
        item->setData(Qt::UserRole + 2, entry.inLibrary);
        item->setToolTip(entry.inProject && entry.inLibrary
                              ? tr("In dieser Show und in der Bibliothek")
                              : entry.inProject ? tr("Nur in dieser Show")
                                                 : tr("Nur in der Bibliothek - wird beim Bearbeiten in diese Show "
                                                      "übernommen"));
        list_->addItem(item);
    }
    selectType(select);
    if (list_->currentRow() < 0 && list_->count() > 0) {
        list_->setCurrentRow(0);
    }
    showSelected();
}

void FixtureLibraryPage::showSelected() {
    const QListWidgetItem* item = list_->currentItem();
    if (item == nullptr) {
        name_->clear();
        name_->setEnabled(false);
        duplicate_->setEnabled(false);
        remove_->setEnabled(false);
        saveToLibrary_->setEnabled(false);
        deleteFromLibrary_->setEnabled(false);
        form_->setVisible(false);
        usage_->setText(tr("Lege mit „Neu“ ein Fixture an. Im Reiter Layout patchst du es dann auf die Bühne."));
        mapping::MatrixShape none;
        none.columns = 0;  // ungültig: die Vorschau zeigt dann nichts
        preview_->setShape(none);
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    const bool inProject = item->data(Qt::UserRole + 1).toBool();
    const bool inLibrary = item->data(Qt::UserRole + 2).toBool();
    name_->setEnabled(true);
    duplicate_->setEnabled(true);
    remove_->setEnabled(inProject);
    saveToLibrary_->setEnabled(inProject);
    deleteFromLibrary_->setEnabled(inLibrary);
    form_->setVisible(true);
    name_->setText(name);
    mapping::FixtureShape shape;
    bool found = false;
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (fromStd(type.name) == name) {
            shape = type.shape;
            found = true;
        }
    }
    if (!found) {
        const auto stored = userlib::load(player_.registry());
        if (stored.ok()) {
            for (const project::FixtureType& type : stored.value()) {
                if (fromStd(type.name) == name) {
                    shape = type.shape;
                    found = true;
                }
            }
        }
    }
    if (!found) {
        return;
    }
    if (inProject) {
        int uses = 0;
        for (const mapping::Fixture& fixture : player_.project()->layout.fixtures()) {
            uses += fromStd(fixture.type) == name ? 1 : 0;
        }
        usage_->setText(uses == 0 ? tr("Noch nicht im Layout gepatcht.")
                                  : tr("Im Layout %1 Mal gepatcht; Änderungen gelten für alle.").arg(uses));
    } else {
        usage_->setText(tr("Nur in der Bibliothek gespeichert. Jede Änderung übernimmt es zuerst in diese Show."));
    }
    form_->setShape(shape);
    preview_->setShape(shape);
}

QString FixtureLibraryPage::uniqueName(const QString& base) const {
    const auto taken = [this](const QString& candidate) {
        return std::ranges::any_of(mergedFixtures(player_),
                                   [&](const FixtureEntry& e) { return e.name == candidate; });
    };
    QString candidate = base;
    for (int n = 2; taken(candidate); ++n) {
        candidate = QStringLiteral("%1 %2").arg(base).arg(n);
    }
    return candidate;
}

void FixtureLibraryPage::report(const Result<void>& result) {
    if (result.ok()) {
        Q_EMIT edited();
        return;
    }
    Q_EMIT failed(fromStd(result.error().message));
    QTimer::singleShot(0, this, [this]() { showSelected(); });  // Anzeige auf den gespeicherten Wert zurück
}

bool FixtureLibraryPage::ensureInProject(const QString& name) {
    if (std::ranges::any_of(player_.fixtureTypes(),
                             [&](const project::FixtureType& t) { return fromStd(t.name) == name; })) {
        return true;
    }
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return false;
        }
    }
    const auto stored = userlib::load(player_.registry());
    if (!stored.ok()) {
        Q_EMIT failed(fromStd(stored.error().message));
        return false;
    }
    for (const project::FixtureType& type : stored.value()) {
        if (fromStd(type.name) == name) {
            const Result<void> added = player_.addFixtureType(type.name, type.shape);
            if (!added.ok()) {
                Q_EMIT failed(fromStd(added.error().message));
                return false;
            }
            return true;
        }
    }
    return false;
}

bool FixtureLibraryPage::addType(Kind kind) {
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return false;
        }
    }
    const Preset preset = presetOf(kind);
    const QString name = uniqueName(preset.name);
    const Result<void> result = player_.addFixtureType(name.toUtf8().toStdString(), preset.shape);
    if (!result.ok()) {
        report(result);
        return false;
    }
    editing_ = true;
    fillList(name);
    editing_ = false;
    Q_EMIT edited();
    return true;
}

void FixtureLibraryPage::duplicateSelected() {
    const QString name = selectedName();
    if (name.isEmpty()) {
        return;
    }
    for (const FixtureEntry& entry : mergedFixtures(player_)) {
        if (entry.name == name) {
            if (player_.project() == nullptr) {
                Q_EMIT projectNeeded();
                if (player_.project() == nullptr) {
                    return;
                }
            }
            const QString copy = uniqueName(name);
            if (player_.addFixtureType(copy.toUtf8().toStdString(), entry.shape).ok()) {
                editing_ = true;
                fillList(copy);
                editing_ = false;
                Q_EMIT edited();
            }
            return;
        }
    }
}

void FixtureLibraryPage::removeSelected() {
    const QString name = selectedName();
    if (name.isEmpty()) {
        return;
    }
    const int row = list_->currentRow();
    const Result<void> result = player_.removeFixtureType(name.toUtf8().toStdString());
    if (!result.ok()) {
        report(result);
        return;
    }
    editing_ = true;
    fillList(QString{});
    list_->setCurrentRow(std::min(row, list_->count() - 1));
    editing_ = false;
    Q_EMIT edited();
}

void FixtureLibraryPage::renameSelected(const QString& newName) {
    const QString name = selectedName();
    if (name.isEmpty() || newName == name || newName.isEmpty()) {
        return;
    }
    if (!ensureInProject(name)) {
        QTimer::singleShot(0, this, [this]() { showSelected(); });
        return;
    }
    const Result<void> result = player_.renameFixtureType(name.toUtf8().toStdString(), newName.toUtf8().toStdString());
    if (!result.ok()) {
        report(result);
        return;
    }
    editing_ = true;
    fillList(newName);
    editing_ = false;
    Q_EMIT edited();
}

void FixtureLibraryPage::applyShape(const mapping::FixtureShape& shape) {
    const QString name = selectedName();
    if (name.isEmpty()) {
        return;
    }
    if (!ensureInProject(name)) {
        return;
    }
    editing_ = true;
    const Result<void> result = player_.replaceFixtureType(name.toUtf8().toStdString(), shape);
    if (result.ok()) {
        preview_->setShape(shape);
        if (QListWidgetItem* item = list_->currentItem(); item != nullptr && item->data(Qt::UserRole).toString() == name) {
            item->setText(tr("%1 (%2 LEDs)").arg(name).arg(mapping::ledCount(shape)));
            item->setData(Qt::UserRole + 1, true);
        }
        remove_->setEnabled(true);
        saveToLibrary_->setEnabled(true);
    }
    report(result);
    editing_ = false;
}

bool FixtureLibraryPage::saveSelectedToLibrary() {
    const QString name = selectedName();
    if (player_.project() == nullptr || name.isEmpty()) {
        Q_EMIT failed(tr("Wähle zuerst ein Fixture in der Liste „Fixtures“."));
        return false;
    }
    const std::string key = name.toUtf8().toStdString();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (type.name == key) {
            const Result<void> saved = userlib::upsert(player_.registry(), type);
            if (!saved.ok()) {
                Q_EMIT failed(fromStd(saved.error().message));
                return false;
            }
            editing_ = true;
            fillList(name);
            editing_ = false;
            Q_EMIT notice(tr("Fixture „%1“ in der Bibliothek gespeichert.").arg(name));
            return true;
        }
    }
    return false;
}

void FixtureLibraryPage::removeFromLibrary() {
    const QString name = selectedName();
    if (name.isEmpty()) {
        return;
    }
    const Result<void> removed = userlib::remove(player_.registry(), name.toUtf8().toStdString());
    if (!removed.ok()) {
        Q_EMIT failed(fromStd(removed.error().message));
        return;
    }
    editing_ = true;
    fillList(name);
    editing_ = false;
}

}  // namespace whitehole::studio
