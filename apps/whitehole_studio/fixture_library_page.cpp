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
            return {QObject::tr("Säule 60"), h};
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
      import_{new QPushButton{tr("Aus Benutzerbibliothek ergänzen"), this}} {
    auto* split = new QSplitter{Qt::Horizontal, this};
    auto* outer = new QHBoxLayout{this};
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addWidget(split);

    auto* left = new QWidget;
    auto* leftColumn = new QVBoxLayout{left};
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->addWidget(new QLabel{tr("<b>Bauformen</b>")});
    leftColumn->addWidget(list_, 1);
    auto* menu = new QMenu{addButton_};
    const struct {
        const char* text;
        Kind kind;
    } kinds[] = {{QT_TR_NOOP("2D-Matrix"), Kind::Matrix2d}, {QT_TR_NOOP("3D-Matrix"), Kind::Matrix3d},
                 {QT_TR_NOOP("Kreis"), Kind::Circle},      {QT_TR_NOOP("Strip"), Kind::Strip},
                 {QT_TR_NOOP("Säule"), Kind::Column}};
    for (const auto& entry : kinds) {
        const Kind kind = entry.kind;
        QAction* action = menu->addAction(tr(entry.text));
        connect(action, &QAction::triggered, this, [this, kind]() { addType(kind); });
    }
    addButton_->setMenu(menu);
    addButton_->setToolTip(tr("Neue Bauform anlegen"));
    duplicate_->setToolTip(tr("Kopie der gewählten Bauform anlegen"));
    remove_->setToolTip(tr("Bauform entfernen. Fixtures im Layout behalten ihre Form als eigene."));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(addButton_);
    buttons->addWidget(duplicate_);
    buttons->addWidget(remove_);
    leftColumn->addLayout(buttons);
    import_->setToolTip(tr("Bauformen, die du früher angelegt hast und die in diesem Projekt fehlen, hinzufügen. "
                           "Gespeichert in: %1").arg(userlib::path()));
    leftColumn->addWidget(import_);
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
    split->setSizes({180, 320, 420});

    connect(list_, &QListWidget::currentRowChanged, this, [this](int) { showSelected(); });
    connect(duplicate_, &QPushButton::clicked, this, [this]() { duplicateSelected(); });
    connect(remove_, &QPushButton::clicked, this, [this]() { removeSelected(); });
    connect(import_, &QPushButton::clicked, this, [this]() { importFromUserLibrary(); });
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
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        auto* item = new QListWidgetItem{tr("%1 (%2 LEDs)").arg(fromStd(type.name)).arg(mapping::ledCount(type.shape))};
        item->setData(Qt::UserRole, fromStd(type.name));
        list_->addItem(item);
    }
    selectType(select);
    if (list_->currentRow() < 0 && list_->count() > 0) {
        list_->setCurrentRow(0);
    }
    showSelected();
}

void FixtureLibraryPage::showSelected() {
    const QString name = selectedName();
    const project::FixtureType* shown = nullptr;
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (fromStd(type.name) == name) {
            shown = &type;
        }
    }
    const bool any = shown != nullptr;
    name_->setEnabled(any);
    duplicate_->setEnabled(any);
    remove_->setEnabled(any);
    form_->setVisible(any);
    if (!any) {
        name_->clear();
        usage_->setText(tr("Lege mit „Neu“ eine Bauform an. Im Reiter Layout verteilst du sie dann auf der Bühne."));
        mapping::MatrixShape none;
        none.columns = 0;  // ungültig: die Vorschau zeigt dann nichts
        preview_->setShape(none);
        return;
    }
    int uses = 0;
    for (const mapping::Fixture& fixture : player_.project()->layout.fixtures()) {
        uses += fixture.type == shown->name ? 1 : 0;
    }
    name_->setText(name);
    usage_->setText(uses == 0 ? tr("Noch nicht im Layout verwendet.")
                              : tr("Im Layout %1 Mal verwendet; Änderungen gelten für alle.").arg(uses));
    form_->setShape(shown->shape);
    preview_->setShape(shown->shape);
}

QString FixtureLibraryPage::uniqueName(const QString& base) const {
    const auto taken = [this](const QString& candidate) {
        return std::ranges::any_of(player_.fixtureTypes(),
                                   [&](const project::FixtureType& t) { return fromStd(t.name) == candidate; });
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
    mirrorToUserLibrary(name);
    editing_ = true;
    fillList(name);
    editing_ = false;
    Q_EMIT edited();
    return true;
}

void FixtureLibraryPage::duplicateSelected() {
    const QString name = selectedName();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (fromStd(type.name) == name) {
            const QString copy = uniqueName(name);
            if (player_.addFixtureType(copy.toUtf8().toStdString(), type.shape).ok()) {
                mirrorToUserLibrary(copy);
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
    mirrorRemoval(name);
    editing_ = true;
    fillList(QString{});
    list_->setCurrentRow(std::min(row, list_->count() - 1));
    editing_ = false;
    Q_EMIT edited();
}

void FixtureLibraryPage::renameSelected(const QString& newName) {
    const QString name = selectedName();
    if (name.isEmpty() || newName == name) {
        return;
    }
    const Result<void> result = player_.renameFixtureType(name.toUtf8().toStdString(), newName.toUtf8().toStdString());
    if (!result.ok()) {
        report(result);
        return;
    }
    mirrorRename(name, newName);
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
    editing_ = true;
    const Result<void> result = player_.replaceFixtureType(name.toUtf8().toStdString(), shape);
    if (result.ok()) {
        mirrorToUserLibrary(name);
        preview_->setShape(shape);
        if (QListWidgetItem* item = list_->currentItem()) {
            item->setText(tr("%1 (%2 LEDs)").arg(name).arg(mapping::ledCount(shape)));
        }
    }
    report(result);
    editing_ = false;
}

}  // namespace whitehole::studio
