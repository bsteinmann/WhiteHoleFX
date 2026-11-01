// node_defaults_page.cpp: Umsetzung zu node_defaults_page.hpp. Typliste, Parameterzeilen und "↺" je Zeile.
#include "node_defaults_page.hpp"

#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <vector>

#include "parameter_editors.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

constexpr int kTypeRole = Qt::UserRole;

}  // namespace

NodeDefaultsPage::NodeDefaultsPage(const graph::INodeRegistry& registry, const runtime::NodeDefaults& current,
                                   QWidget* parent)
    : QWidget{parent}, registry_{registry}, defaults_{current} {
    auto* box = new QVBoxLayout{this};
    auto* intro = new QLabel{tr("Neue Nodes starten mit diesen Werten statt mit denen der Bibliothek. Bestehende Nodes "
                                "und geladene Projekte ändern sich nicht. Fett markierte Typen haben eigene Werte."),
                             this};
    intro->setWordWrap(true);
    box->addWidget(intro);

    auto* split = new QSplitter{Qt::Horizontal, this};
    auto* left = new QWidget{split};
    auto* leftBox = new QVBoxLayout{left};
    leftBox->setContentsMargins(0, 0, 0, 0);
    filter_ = new QLineEdit{left};
    filter_->setPlaceholderText(tr("Node-Typ suchen …"));
    filter_->setClearButtonEnabled(true);
    tree_ = new QTreeWidget{left};
    tree_->setHeaderHidden(true);
    leftBox->addWidget(filter_);
    leftBox->addWidget(tree_, 1);
    rows_ = new QScrollArea{split};
    rows_->setWidgetResizable(true);
    rows_->setFrameShape(QFrame::NoFrame);
    split->addWidget(left);
    split->addWidget(rows_);
    split->setStretchFactor(1, 1);
    box->addWidget(split, 1);

    auto* reset = new QPushButton{tr("Alle Standardwerte zurücksetzen"), this};
    connect(reset, &QPushButton::clicked, this, [this]() { resetAll(); });
    box->addWidget(reset, 0, Qt::AlignRight);

    // Nur Typen mit Parametern; nach Kategorie, innerhalb nach Anzeigename.
    std::vector<const graph::NodeDescriptor*> types;
    for (const graph::NodeDescriptor* descriptor : registry_.all()) {
        if (!descriptor->parameters.empty()) {
            types.push_back(descriptor);
        }
    }
    std::sort(types.begin(), types.end(), [](const graph::NodeDescriptor* a, const graph::NodeDescriptor* b) {
        return a->category != b->category ? a->category < b->category : a->displayName < b->displayName;
    });
    std::map<std::string, QTreeWidgetItem*> categories;
    for (const graph::NodeDescriptor* descriptor : types) {
        QTreeWidgetItem*& category = categories[descriptor->category];
        if (category == nullptr) {
            category = new QTreeWidgetItem{tree_, QStringList{fromStd(descriptor->category)}};
            category->setFlags(Qt::ItemIsEnabled);
        }
        auto* item = new QTreeWidgetItem{category, QStringList{fromStd(descriptor->displayName)}};
        item->setData(0, kTypeRole, fromStd(descriptor->typeId));
        items_[descriptor->typeId] = item;
    }
    tree_->expandAll();
    for (const auto& entry : items_) {
        markType(entry.first);
    }

    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (item != nullptr && item->data(0, kTypeRole).isValid()) {
            showType(item->data(0, kTypeRole).toString());
        }
    });
    connect(filter_, &QLineEdit::textChanged, this, [this](const QString& text) { applyFilter(text); });
}

int NodeDefaultsPage::showType(const QString& typeId) {
    const graph::NodeDescriptor* descriptor = registry_.find(typeId.toStdString());
    if (descriptor == nullptr) {
        return 0;
    }
    rebuildRows(*descriptor);
    return static_cast<int>(descriptor->parameters.size());
}

void NodeDefaultsPage::rebuildRows(const graph::NodeDescriptor& descriptor) {
    auto* page = new QWidget;
    auto* form = new QFormLayout{page};
    const std::string typeId = descriptor.typeId;
    for (const graph::ParameterDescriptor& parameter : descriptor.parameters) {
        runtime::ParameterInfo info;
        info.key = parameter.key;
        info.displayName = parameter.displayName;
        info.defaultValue = parameter.defaultValue;
        info.minValue = parameter.minValue;
        info.maxValue = parameter.maxValue;
        info.choices = parameter.choices;
        const graph::ParameterValue* own = defaults_.find(typeId, parameter.key);
        info.value = own != nullptr ? *own : parameter.defaultValue;

        auto* row = new QWidget{page};
        auto* rowBox = new QHBoxLayout{row};
        rowBox->setContentsMargins(0, 0, 0, 0);
        auto* undo = new QToolButton{row};
        undo->setText(QStringLiteral("↺"));
        undo->setToolTip(tr("Auf den Wert der Bibliothek zurückstellen"));
        undo->setEnabled(own != nullptr);

        // Der Parameter lebt im Deskriptor der Registry, der länger lebt als diese Seite.
        const graph::ParameterDescriptor* described = &parameter;
        Editor editor = makeEditor(
            info,
            [this, typeId, described, undo](graph::ParameterValue value) {
                store(typeId, *described, value);
                undo->setEnabled(defaults_.find(typeId, described->key) != nullptr);
            },
            row, std::nullopt);
        connect(undo, &QToolButton::clicked, this, [this, typeId, described, undo, show = editor.show]() {
            defaults_.clear(typeId, described->key);
            show(described->defaultValue);
            undo->setEnabled(false);
            markType(typeId);
        });
        rowBox->addWidget(editor.widget, 1);
        rowBox->addWidget(undo);
        form->addRow(fromStd(parameter.displayName), row);
    }
    // Das alte Widget wird mit dem Setzen des neuen frei; deleteLater, weil sein Signal gerade laufen kann.
    QWidget* old = rows_->takeWidget();
    rows_->setWidget(page);
    if (old != nullptr) {
        old->deleteLater();
    }
}

void NodeDefaultsPage::store(const std::string& typeId, const graph::ParameterDescriptor& parameter,
                             const graph::ParameterValue& value) {
    if (value == parameter.defaultValue) {
        defaults_.clear(typeId, parameter.key);  // gleich dem Bibliothekswert: kein eigener Standardwert
    } else {
        defaults_.set(typeId, parameter.key, value);
    }
    markType(typeId);
}

void NodeDefaultsPage::markType(const std::string& typeId) {
    const auto found = items_.find(typeId);
    if (found == items_.end()) {
        return;
    }
    QFont font = found->second->font(0);
    font.setBold(defaults_.valuesOf(typeId) != nullptr);
    found->second->setFont(0, font);
}

void NodeDefaultsPage::applyFilter(const QString& text) {
    const QString needle = text.trimmed();
    for (int c = 0; c < tree_->topLevelItemCount(); ++c) {
        QTreeWidgetItem* category = tree_->topLevelItem(c);
        bool any = false;
        for (int i = 0; i < category->childCount(); ++i) {
            QTreeWidgetItem* item = category->child(i);
            const bool match = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive) ||
                               item->data(0, kTypeRole).toString().contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
            any = any || match;
        }
        category->setHidden(!any);
    }
}

void NodeDefaultsPage::resetAll() {
    defaults_.clearAll();
    for (const auto& entry : items_) {
        markType(entry.first);
    }
    if (const QTreeWidgetItem* current = tree_->currentItem();
        current != nullptr && current->data(0, kTypeRole).isValid()) {
        showType(current->data(0, kTypeRole).toString());
    }
}

}  // namespace whitehole::studio
