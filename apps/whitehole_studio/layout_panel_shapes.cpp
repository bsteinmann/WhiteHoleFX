// layout_panel_shapes.cpp: Bauform-Bearbeitung im Reiter Layout (früher eigene Seite "Fixtures", FixtureLibraryPage).
//
// Warum hierher gezogen (vom Nutzer gewünscht): die Seite "Fixtures" im Stage-Config-Fenster zeigt jetzt echte
// Fixture-Exemplare statt Bauformen (siehe fixtures_page.hpp) - das Anlegen und Bearbeiten einer Bauform selbst
// (Maße, Verkabelung, Vorschau) gehört jetzt hierher, zur Fixture-Bibliothek. Eine Bauform (project::FixtureType)
// wird hier einmal beschrieben; "Fixtures auf der Bühne" (layout_panel.cpp) legt beliebig oft ein Exemplar davon
// an. Ändert man eine Bauform, folgen alle Exemplare (Player::replaceFixtureType); entfernt man sie, behalten die
// Exemplare ihre Form als eigene.
//
// Eigene Datei wegen der 400-Zeilen-Grenze (layout_panel.cpp).
#include "layout_panel.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

struct ShapePreset {
    QString name;
    mapping::FixtureShape shape;
};

ShapePreset presetOf(LayoutPanel::ShapeKind kind) {
    using Kind = LayoutPanel::ShapeKind;
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

QWidget* LayoutPanel::buildShapesArea() {
    auto* area = new QWidget;
    auto* outer = new QHBoxLayout{area};
    outer->setContentsMargins(0, 0, 0, 0);

    auto* left = new QWidget;
    auto* leftColumn = new QVBoxLayout{left};
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->addWidget(new QLabel{tr("<b>Bauformen dieser Show</b>")});
    shapeList_ = new QListWidget;
    leftColumn->addWidget(shapeList_, 1);
    shapeAddButton_ = new QPushButton{tr("Neu")};
    shapeDuplicate_ = new QPushButton{tr("Duplizieren")};
    shapeRemove_ = new QPushButton{tr("Entfernen")};
    auto* menu = new QMenu{shapeAddButton_};
    const struct {
        const char* text;
        ShapeKind kind;
    } kinds[] = {{QT_TR_NOOP("2D-Matrix"), ShapeKind::Matrix2d}, {QT_TR_NOOP("3D-Matrix"), ShapeKind::Matrix3d},
                 {QT_TR_NOOP("Kreis"), ShapeKind::Circle},      {QT_TR_NOOP("Strip"), ShapeKind::Strip},
                 {QT_TR_NOOP("Säule (Ringe)"), ShapeKind::Cylinder}, {QT_TR_NOOP("Spirale (Säule)"), ShapeKind::Column}};
    for (const auto& entry : kinds) {
        const ShapeKind kind = entry.kind;
        QAction* action = menu->addAction(tr(entry.text));
        connect(action, &QAction::triggered, this, [this, kind]() { addShapeType(kind); });
    }
    shapeAddButton_->setMenu(menu);
    shapeAddButton_->setToolTip(tr("Neue Bauform anlegen"));
    shapeDuplicate_->setToolTip(tr("Kopie der gewählten Bauform anlegen"));
    shapeRemove_->setToolTip(tr("Bauform entfernen. Fixtures im Layout behalten ihre Form als eigene."));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(shapeAddButton_);
    buttons->addWidget(shapeDuplicate_);
    buttons->addWidget(shapeRemove_);
    leftColumn->addLayout(buttons);
    catalogSave_ = new QPushButton{tr("In Bibliothek speichern")};
    catalogSave_->setToolTip(tr("Speichert die gewählte Bauform in der Bibliothek (Benutzerverzeichnis), damit du sie in "
                                "anderen Shows wieder holen kannst. Gespeichert in: %1")
                                  .arg(userlib::path()));
    leftColumn->addWidget(catalogSave_);
    outer->addWidget(left, 1);

    auto* middle = new QWidget;
    auto* middleColumn = new QVBoxLayout{middle};
    middleColumn->setContentsMargins(0, 0, 0, 0);
    shapeName_ = new QLineEdit;
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel{tr("Name")});
    nameRow->addWidget(shapeName_, 1);
    middleColumn->addLayout(nameRow);
    shapeUsage_ = new QLabel;
    middleColumn->addWidget(shapeUsage_);
    shapeForm_ = new ShapeForm;
    auto* formScroll = new QScrollArea;
    formScroll->setWidgetResizable(true);
    formScroll->setWidget(shapeForm_);
    middleColumn->addWidget(formScroll, 1);
    outer->addWidget(middle, 1);

    shapePreview_ = new ShapePreview;
    outer->addWidget(shapePreview_, 1);

    connect(shapeList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            const QSignalBlocker blocker{catalog_};
            catalog_->setCurrentRow(-1);  // es ist immer nur eine der beiden Listen gewählt
            catalogTakeOver_->setEnabled(false);
            catalogDelete_->setEnabled(false);
        }
        showSelectedShape();
    });
    connect(shapeDuplicate_, &QPushButton::clicked, this, [this]() { duplicateSelectedShape(); });
    connect(shapeRemove_, &QPushButton::clicked, this, [this]() { removeSelectedShape(); });
    connect(catalogSave_, &QPushButton::clicked, this, [this]() { saveSelectedShapeToLibrary(); });
    connect(shapeName_, &QLineEdit::editingFinished, this, [this]() { renameSelectedShape(shapeName_->text()); });
    connect(shapeForm_, &ShapeForm::shapeEdited, this, [this](const mapping::FixtureShape& shape) { applyShapeEdit(shape); });
    return area;
}

QString LayoutPanel::selectedShapeName() const {
    const QListWidgetItem* item = shapeList_->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString{};
}

void LayoutPanel::selectShapeType(const QString& name) {
    for (int row = 0; row < shapeList_->count(); ++row) {
        if (shapeList_->item(row)->data(Qt::UserRole).toString() == name) {
            shapeList_->setCurrentRow(row);
            return;
        }
    }
}

void LayoutPanel::fillShapeList(const QString& select) {
    const QSignalBlocker blocker{shapeList_};
    shapeList_->clear();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        auto* item =
            new QListWidgetItem{tr("%1 (%2 LEDs)").arg(fromStd(type.name)).arg(mapping::ledCount(type.shape))};
        item->setData(Qt::UserRole, fromStd(type.name));
        shapeList_->addItem(item);
    }
    selectShapeType(select);
    if (shapeList_->currentRow() < 0 && shapeList_->count() > 0 && catalog_->currentRow() < 0) {
        shapeList_->setCurrentRow(0);
    }
    showSelectedShape();
}

void LayoutPanel::showSelectedShape() {
    const QString name = selectedShapeName();
    const project::FixtureType* shown = nullptr;
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (fromStd(type.name) == name) {
            shown = &type;
        }
    }
    const bool any = shown != nullptr;
    shapeName_->setEnabled(any);
    shapeDuplicate_->setEnabled(any);
    shapeRemove_->setEnabled(any);
    catalogSave_->setEnabled(any);
    shapeForm_->setVisible(any);
    if (!any && catalog_->currentRow() >= 0) {
        return;  // es wird gerade eine gespeicherte Bauform gezeigt (showSelectedCatalog, layout_panel_catalog.cpp)
    }
    if (!any) {
        shapeName_->clear();
        shapeUsage_->setText(tr("Lege mit „Neu“ eine Bauform an. Darunter verteilst du sie auf der Bühne."));
        mapping::MatrixShape none;
        none.columns = 0;  // ungültig: die Vorschau zeigt dann nichts
        shapePreview_->setShape(none);
        return;
    }
    int uses = 0;
    for (const mapping::Fixture& fixture : player_.project()->layout.fixtures()) {
        uses += fixture.type == shown->name ? 1 : 0;
    }
    shapeName_->setText(name);
    shapeUsage_->setText(uses == 0 ? tr("Noch nicht im Layout verwendet.")
                                   : tr("Im Layout %1 Mal verwendet; Änderungen gelten für alle.").arg(uses));
    shapeForm_->setShape(shown->shape);
    shapePreview_->setShape(shown->shape);
}

QString LayoutPanel::uniqueShapeName(const QString& base) const {
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

void LayoutPanel::reportShape(const Result<void>& result) {
    if (result.ok()) {
        Q_EMIT edited();
        return;
    }
    Q_EMIT failed(fromStd(result.error().message));
    QTimer::singleShot(0, this, [this]() { showSelectedShape(); });  // Anzeige auf den gespeicherten Wert zurück
}

bool LayoutPanel::addShapeType(ShapeKind kind) {
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return false;
        }
    }
    const ShapePreset preset = presetOf(kind);
    const QString name = uniqueShapeName(preset.name);
    const Result<void> result = player_.addFixtureType(name.toUtf8().toStdString(), preset.shape);
    if (!result.ok()) {
        reportShape(result);
        return false;
    }
    shapeEditing_ = true;
    fillShapeList(name);
    shapeEditing_ = false;
    Q_EMIT edited();
    return true;
}

void LayoutPanel::duplicateSelectedShape() {
    const QString name = selectedShapeName();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (fromStd(type.name) == name) {
            const QString copy = uniqueShapeName(name);
            if (player_.addFixtureType(copy.toUtf8().toStdString(), type.shape).ok()) {
                shapeEditing_ = true;
                fillShapeList(copy);
                shapeEditing_ = false;
                Q_EMIT edited();
            }
            return;
        }
    }
}

void LayoutPanel::removeSelectedShape() {
    const QString name = selectedShapeName();
    if (name.isEmpty()) {
        return;
    }
    const int row = shapeList_->currentRow();
    const Result<void> result = player_.removeFixtureType(name.toUtf8().toStdString());
    if (!result.ok()) {
        reportShape(result);
        return;
    }
    shapeEditing_ = true;
    fillShapeList(QString{});
    shapeList_->setCurrentRow(std::min(row, shapeList_->count() - 1));
    shapeEditing_ = false;
    Q_EMIT edited();
}

void LayoutPanel::renameSelectedShape(const QString& newName) {
    const QString name = selectedShapeName();
    if (name.isEmpty() || newName == name) {
        return;
    }
    const Result<void> result = player_.renameFixtureType(name.toUtf8().toStdString(), newName.toUtf8().toStdString());
    if (!result.ok()) {
        reportShape(result);
        return;
    }
    shapeEditing_ = true;
    fillShapeList(newName);
    shapeEditing_ = false;
    Q_EMIT edited();
}

void LayoutPanel::applyShapeEdit(const mapping::FixtureShape& shape) {
    const QString name = selectedShapeName();
    if (name.isEmpty()) {
        return;
    }
    shapeEditing_ = true;
    const Result<void> result = player_.replaceFixtureType(name.toUtf8().toStdString(), shape);
    if (result.ok()) {
        shapePreview_->setShape(shape);
        if (QListWidgetItem* item = shapeList_->currentItem()) {
            item->setText(tr("%1 (%2 LEDs)").arg(name).arg(mapping::ledCount(shape)));
        }
    }
    reportShape(result);
    shapeEditing_ = false;
}

}  // namespace whitehole::studio
