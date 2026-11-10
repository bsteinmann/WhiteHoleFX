// node_palette_library.cpp: Die Bibliothek in der Palette (Rubriken "Gruppen" und "Presets", Teil von NodePalette).
//
// Warum es das gibt: Presets und Gruppen sollen dort liegen, wo man Nodes sucht. Die Einträge sind Dateien in zwei
// Ordnern (runtime/node_preset.hpp); die Palette liest sie neu ein, wenn der Editor etwas speichert oder löscht.
#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "node_palette.hpp"
#include "whitehole/runtime/node_preset.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

/// Alle Wörter des Filters kommen im Namen vor (Groß-/Kleinschreibung egal).
bool matchesFilter(const QString& name, const QString& filter) {
    const QStringList words = filter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& word : words) {
        if (!name.contains(word, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

}  // namespace

void NodePalette::setLibrary(const runtime::PresetStore* presets, const runtime::PresetStore* groups) {
    presetStore_ = presets;
    groupStore_ = groups;
    refreshLibrary();
}

void NodePalette::refreshLibrary() { fill(search_->text()); }

void NodePalette::fillLibrary(bool filtered, const QString& filter, const QFont& bold) {
    struct Section {
        QString title;
        QString kind;
        const runtime::PresetStore* store;
        QString empty;
        QString tip;
    };
    const Section sections[] = {
        {tr("Gruppen"), QStringLiteral("group"), groupStore_, tr("(noch keine Gruppen)"),
         tr("Doppelklick oder auf die Fläche ziehen: Gruppe einfügen\nRechtsklick: löschen")},
        {tr("Presets"), QStringLiteral("preset"), presetStore_, tr("(noch keine Presets)"),
         tr("Doppelklick oder auf die Fläche ziehen: Nodes einfügen\nRechtsklick: löschen")},
    };
    for (const Section& section : sections) {
        if (section.store == nullptr) {
            continue;
        }
        QStringList names;
        for (const std::string& name : section.store->list()) {
            if (const QString text = fromStd(name); !filtered || matchesFilter(text, filter)) {
                names << text;
            }
        }
        if (filtered && names.isEmpty()) {
            continue;
        }
        auto* category = new QTreeWidgetItem{tree_};
        category->setText(0, section.title);
        category->setFont(0, bold);
        category->setFlags(Qt::ItemIsEnabled);
        category->setData(0, kLibraryKindRole, section.kind);
        category->setExpanded(filtered || expanded_.contains(section.title));
        if (names.isEmpty()) {
            auto* none = new QTreeWidgetItem{category};
            none->setText(0, section.empty);
            none->setFlags(Qt::NoItemFlags);
        }
        for (const QString& name : names) {
            auto* item = new QTreeWidgetItem{category};
            item->setText(0, name);
            item->setToolTip(0, section.tip);
            item->setData(0, kLibraryKindRole, section.kind);
            item->setData(0, kLibraryNameRole, name);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        }
    }
}

void NodePalette::showLibraryMenu(const QTreeWidgetItem& item, const QPoint& globalPosition) {
    const QString kind = item.data(0, kLibraryKindRole).toString();
    const QString name = item.data(0, kLibraryNameRole).toString();
    const runtime::PresetStore* store = kind == QStringLiteral("group") ? groupStore_ : presetStore_;
    if (store == nullptr) {
        return;
    }
    QMenu menu{this};
    QAction* add = menu.addAction(tr("Einfügen"));
    QAction* remove = menu.addAction(kind == QStringLiteral("group") ? tr("Gruppe aus der Bibliothek löschen …")
                                                                      : tr("Preset löschen …"));
    QAction* chosen = menu.exec(globalPosition);
    if (chosen == add) {
        Q_EMIT libraryAddRequested(kind, name);
    } else if (chosen == remove &&
               QMessageBox::question(this, tr("Löschen"), tr("\"%1\" aus der Bibliothek löschen?").arg(name),
                                     QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes) {
        (void)store->remove(name.toUtf8().toStdString());
        refreshLibrary();
    }
}

int NodePalette::libraryEntryCount() const {
    int count = 0;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* category = tree_->topLevelItem(i);
        if (category->data(0, kLibraryKindRole).isNull()) {
            continue;
        }
        for (int j = 0; j < category->childCount(); ++j) {
            count += category->child(j)->data(0, kLibraryNameRole).toString().isEmpty() ? 0 : 1;
        }
    }
    return count;
}

}  // namespace whitehole::studio
