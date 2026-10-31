// look_panel.cpp: Look-Liste mit Speichern, Bearbeiten, Ordnen.
#include "look_panel.hpp"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

#include "whitehole/runtime/live_look.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

LookPanel::LookPanel(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(6, 6, 6, 6);

    auto* intro = new QLabel{tr("Ein Look ist ein fertiger Effekt (vom Generator bis zum Ausgang). "
                                "Speichere deine Effekte hier und rufe sie im Live-Modus mit einem Klick ab."),
                             this};
    intro->setWordWrap(true);
    layout->addWidget(intro);

    saveButton_ = new QPushButton{tr("Aktuellen Effekt als Look speichern …"), this};
    layout->addWidget(saveButton_);

    list_ = new QListWidget{this};
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list_, 1);

    auto* fadeRow = new QHBoxLayout;
    fadeRow->addWidget(new QLabel{tr("Überblendzeit:"), this});
    fade_ = new QDoubleSpinBox{this};
    fade_->setRange(0.0, 600.0);
    fade_->setDecimals(1);
    fade_->setSingleStep(0.5);
    fade_->setSuffix(tr(" s"));
    fade_->setToolTip(tr("So lange dauert das Überblenden zu diesem Look. 0 = hartes Umschalten."));
    fadeRow->addWidget(fade_);
    fadeRow->addStretch(1);
    layout->addLayout(fadeRow);

    auto* grid = new QGridLayout;
    overwriteButton_ = new QPushButton{tr("Mit aktuellem Effekt überschreiben"), this};
    editButton_ = new QPushButton{tr("In den Editor laden"), this};
    renameButton_ = new QPushButton{tr("Umbenennen …"), this};
    removeButton_ = new QPushButton{tr("Löschen"), this};
    upButton_ = new QPushButton{tr("▲ nach oben"), this};
    downButton_ = new QPushButton{tr("▼ nach unten"), this};
    grid->addWidget(overwriteButton_, 0, 0, 1, 2);
    grid->addWidget(editButton_, 1, 0, 1, 2);
    grid->addWidget(renameButton_, 2, 0);
    grid->addWidget(removeButton_, 2, 1);
    grid->addWidget(upButton_, 3, 0);
    grid->addWidget(downButton_, 3, 1);
    layout->addLayout(grid);

    liveButton_ = new QPushButton{tr("Live-Modus starten  ▶"), this};
    liveButton_->setMinimumHeight(40);
    liveButton_->setToolTip(tr("Blendet die Bearbeitung aus und zeigt große Knöpfe für alle Looks (F9)"));
    layout->addWidget(liveButton_);

    connect(saveButton_, &QPushButton::clicked, this, [this]() { askSaveNew(); });
    connect(overwriteButton_, &QPushButton::clicked, this, [this]() { askOverwrite(); });
    connect(editButton_, &QPushButton::clicked, this, [this]() { loadIntoEditor(selectedIndex()); });
    connect(renameButton_, &QPushButton::clicked, this, [this]() { askRename(); });
    connect(removeButton_, &QPushButton::clicked, this, [this]() { askRemove(); });
    connect(upButton_, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
    connect(downButton_, &QPushButton::clicked, this, [this]() { moveSelected(1); });
    connect(liveButton_, &QPushButton::clicked, this, [this]() { Q_EMIT liveRequested(std::max(selectedIndex(), 0)); });
    connect(list_, &QListWidget::currentRowChanged, this, [this](int) { onSelectionChanged(); });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { loadIntoEditor(selectedIndex()); });
    connect(fade_, &QDoubleSpinBox::valueChanged, this, [this](double seconds) {
        const int index = selectedIndex();
        if (updating_ || index < 0) {
            return;
        }
        const Result<void> r = player_.setLookFade(index, seconds);
        if (!r.ok()) {
            Q_EMIT failed(fromStd(r.error().message));
            return;
        }
        rebuild();
        Q_EMIT changed();
    });
    rebuild();
}

int LookPanel::selectedIndex() const { return list_->currentRow(); }

int LookPanel::lookCount() const { return list_->count(); }

void LookPanel::select(int index) {
    if (index >= 0 && index < list_->count()) {
        list_->setCurrentRow(index);
    }
}

void LookPanel::rebuild() {
    const int keep = list_->currentRow();
    updating_ = true;
    {
        const QSignalBlocker blocker{list_};
        list_->clear();
        const auto& looks = player_.looks();
        for (std::size_t i = 0; i < looks.size(); ++i) {
            list_->addItem(QStringLiteral("%1   %2   (%3 s)")
                               .arg(i + 1)
                               .arg(fromStd(looks[i].name))
                               .arg(looks[i].fadeSeconds, 0, 'f', 1));
        }
        if (list_->count() > 0) {
            list_->setCurrentRow(std::clamp(keep, 0, list_->count() - 1));
        }
    }
    updating_ = false;
    onSelectionChanged();
}

void LookPanel::onSelectionChanged() {
    const int index = selectedIndex();
    if (index >= 0 && static_cast<std::size_t>(index) < player_.looks().size()) {
        updating_ = true;
        fade_->setValue(player_.looks()[static_cast<std::size_t>(index)].fadeSeconds);
        updating_ = false;
    }
    updateButtons();
}

void LookPanel::updateButtons() {
    const bool project = player_.project() != nullptr;
    const bool any = selectedIndex() >= 0;
    saveButton_->setEnabled(project && player_.project()->target.has_value());
    overwriteButton_->setEnabled(any && project);
    editButton_->setEnabled(any);
    renameButton_->setEnabled(any);
    removeButton_->setEnabled(any);
    upButton_->setEnabled(any && selectedIndex() > 0);
    downButton_->setEnabled(any && selectedIndex() + 1 < list_->count());
    fade_->setEnabled(any);
    liveButton_->setEnabled(list_->count() > 0);
}

QString LookPanel::suggestedName() const {
    for (int n = list_->count() + 1;; ++n) {
        const QString name = tr("Look %1").arg(n);
        if (runtime::findLook(player_.looks(), name.toUtf8().toStdString()) < 0) {
            return name;
        }
    }
}

// --- Aktionen ohne Dialog -----------------------------------------------------------------------

bool LookPanel::saveLookAs(const QString& name) {
    const auto saved = player_.saveLook(name.toUtf8().toStdString());
    if (!saved.ok()) {
        Q_EMIT failed(fromStd(saved.error().message));
        return false;
    }
    rebuild();
    select(saved.value());
    Q_EMIT changed();
    return true;
}

bool LookPanel::overwriteLook(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= player_.looks().size()) {
        return false;
    }
    return saveLookAs(fromStd(player_.looks()[static_cast<std::size_t>(index)].name));
}

bool LookPanel::loadIntoEditor(int index) {
    const Result<void> r = player_.loadLookIntoEditor(index);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    Q_EMIT loadedIntoEditor();
    Q_EMIT changed();
    return true;
}

bool LookPanel::renameLook(int index, const QString& name) {
    const Result<void> r = player_.renameLook(index, name.toUtf8().toStdString());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    rebuild();
    Q_EMIT changed();
    return true;
}

bool LookPanel::removeLook(int index) {
    const Result<void> r = player_.removeLook(index);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    rebuild();
    Q_EMIT changed();
    return true;
}

bool LookPanel::moveSelected(int delta) {
    const int from = selectedIndex();
    const Result<void> r = player_.moveLook(from, from + delta);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    rebuild();
    select(from + delta);
    Q_EMIT changed();
    return true;
}

// --- Mit Dialog ---------------------------------------------------------------------------------

void LookPanel::askSaveNew() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Look speichern"), tr("Name des Looks:"), QLineEdit::Normal,
                                               suggestedName(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (runtime::findLook(player_.looks(), name.toUtf8().toStdString()) >= 0 &&
        QMessageBox::question(this, tr("Look ersetzen"), tr("Es gibt schon einen Look „%1“. Mit dem aktuellen Effekt überschreiben?").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    saveLookAs(name);
}

void LookPanel::askOverwrite() {
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    const QString name = fromStd(player_.looks()[static_cast<std::size_t>(index)].name);
    if (QMessageBox::question(this, tr("Look überschreiben"), tr("Den Look „%1“ durch den aktuellen Effekt ersetzen?").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes) {
        overwriteLook(index);
    }
}

void LookPanel::askRename() {
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Look umbenennen"), tr("Neuer Name:"), QLineEdit::Normal,
                                               fromStd(player_.looks()[static_cast<std::size_t>(index)].name), &ok)
                             .trimmed();
    if (ok && !name.isEmpty()) {
        renameLook(index, name);
    }
}

void LookPanel::askRemove() {
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    const QString name = fromStd(player_.looks()[static_cast<std::size_t>(index)].name);
    if (QMessageBox::question(this, tr("Look löschen"), tr("Den Look „%1“ löschen?").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes) {
        removeLook(index);
    }
}

}  // namespace whitehole::studio
