// fixture_library_user.cpp: Teil von FixtureLibraryPage: die gespeicherten Bauformen (Benutzerbibliothek auf der Festplatte).
//
// Warum eine eigene Datei: fixture_library_page.cpp bleibt so unter der Größenmarke, und alles, was die Festplatte
// berührt, steht an einer Stelle. Die Seite zeigt zwei Listen: die Fixtures dieser Show (im Projekt) und darunter alle
// gespeicherten (die Datei fixture-library.whfx). Beides ist getrennt: In die Bibliothek kommt eine Bauform nur auf
// Knopfdruck, in die Show nur durch Übernehmen; Entfernen in der einen Liste lässt die andere unberührt.
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>

#include <algorithm>

#include "fixture_library_page.hpp"
#include "form_helpers.hpp"
#include "shape_form.hpp"
#include "shape_preview.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

/// Die gespeicherten Bauformen; ein Lesefehler (defekte Datei) steht in `error`, die Liste bleibt dann leer.
std::vector<project::FixtureType> loadStored(const runtime::Player& player, QString& error) {
    const auto stored = userlib::load(player.registry());
    if (!stored.ok()) {
        error = fromStd(stored.error().message);
        return {};
    }
    return stored.value();
}

}  // namespace

QString FixtureLibraryPage::selectedStoredName() const {
    const QListWidgetItem* item = stored_->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString{};
}

void FixtureLibraryPage::fillStored(const QString& select) {
    const QSignalBlocker blocker{stored_};
    stored_->clear();
    QString error;
    for (const project::FixtureType& type : loadStored(player_, error)) {
        auto* item = new QListWidgetItem{tr("%1 (%2 LEDs)").arg(fromStd(type.name)).arg(mapping::ledCount(type.shape))};
        item->setData(Qt::UserRole, fromStd(type.name));
        stored_->addItem(item);
    }
    for (int row = 0; row < stored_->count(); ++row) {
        if (stored_->item(row)->data(Qt::UserRole).toString() == select) {
            stored_->setCurrentRow(row);
        }
    }
    const bool any = stored_->count() > 0;
    takeOver_->setEnabled(stored_->currentRow() >= 0);
    deleteStored_->setEnabled(stored_->currentRow() >= 0);
    import_->setEnabled(any);
    if (!error.isEmpty()) {
        Q_EMIT failed(error);
    }
    if (stored_->currentRow() >= 0) {
        showStored();
    }
}

void FixtureLibraryPage::showStored() {
    const QString name = selectedStoredName();
    takeOver_->setEnabled(!name.isEmpty());
    deleteStored_->setEnabled(!name.isEmpty());
    if (name.isEmpty()) {
        return;
    }
    QString error;
    for (const project::FixtureType& type : loadStored(player_, error)) {
        if (fromStd(type.name) == name) {
            // Nur ansehen: bearbeitet wird eine Bauform in der Show; „In Bibliothek speichern“ legt sie wieder ab.
            name_->setText(name);
            name_->setEnabled(false);
            duplicate_->setEnabled(false);
            remove_->setEnabled(false);
            saveToLibrary_->setEnabled(false);
            form_->setVisible(false);
            usage_->setText(tr("Gespeichert in der Bibliothek. „In Show übernehmen“ (oder Doppelklick) holt sie in diese Show."));
            preview_->setShape(type.shape);
            return;
        }
    }
}

bool FixtureLibraryPage::saveSelectedToLibrary() {
    const QString name = selectedName();
    if (player_.project() == nullptr || name.isEmpty()) {
        Q_EMIT failed(tr("Wähle zuerst eine Bauform in der Liste „Fixtures“."));
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
            fillStored(name);
            Q_EMIT notice(tr("Bauform „%1“ in der Bibliothek gespeichert.").arg(name));
            return true;
        }
    }
    return false;
}

bool FixtureLibraryPage::takeOverStored(const QString& name) {
    if (name.isEmpty()) {
        return false;
    }
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return false;
        }
    }
    QString error;
    for (const project::FixtureType& type : loadStored(player_, error)) {
        if (fromStd(type.name) != name) {
            continue;
        }
        const auto& have = player_.fixtureTypes();
        if (std::ranges::any_of(have, [&](const project::FixtureType& t) { return t.name == type.name; })) {
            Q_EMIT failed(tr("In dieser Show gibt es schon eine Bauform „%1“.").arg(name));
            return false;
        }
        const Result<void> result = player_.addFixtureType(type.name, type.shape);
        if (!result.ok()) {
            Q_EMIT failed(fromStd(result.error().message));
            return false;
        }
        editing_ = true;
        fillList(name);
        editing_ = false;
        Q_EMIT edited();
        return true;
    }
    if (!error.isEmpty()) {
        Q_EMIT failed(error);
    }
    return false;
}

void FixtureLibraryPage::removeStored(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    const int row = stored_->currentRow();
    const Result<void> removed = userlib::remove(player_.registry(), name.toUtf8().toStdString());
    if (!removed.ok()) {
        Q_EMIT failed(fromStd(removed.error().message));
        return;
    }
    fillStored();
    if (stored_->count() > 0) {
        stored_->setCurrentRow(std::min(row, stored_->count() - 1));
    } else {
        showSelected();
    }
}

int FixtureLibraryPage::importFromUserLibrary() {
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return 0;
        }
    }
    QString error;
    const std::vector<project::FixtureType> stored = loadStored(player_, error);
    if (!error.isEmpty()) {
        Q_EMIT failed(error);
        return 0;
    }
    int added = 0;
    QString last;
    for (const project::FixtureType& type : stored) {
        const auto& have = player_.fixtureTypes();
        if (std::ranges::any_of(have, [&](const project::FixtureType& t) { return t.name == type.name; })) {
            continue;
        }
        const Result<void> result = player_.addFixtureType(type.name, type.shape);
        if (!result.ok()) {
            Q_EMIT failed(fromStd(result.error().message));
            break;
        }
        ++added;
        last = fromStd(type.name);
    }
    if (added > 0) {
        editing_ = true;
        fillList(last);
        editing_ = false;
        Q_EMIT edited();
    }
    return added;
}

}  // namespace whitehole::studio
