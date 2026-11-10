// node_palette.cpp: Baum aller Node-Typen mit Suche und Drag & Drop.
#include "node_palette.hpp"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDrag>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include "node_info.hpp"
#include "node_items.hpp"
#include "node_search.hpp"
#include "whitehole/graph/port_family.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

constexpr int kTypeIdRole = Qt::UserRole;
constexpr int kFavoritesRole = Qt::UserRole + 1;  // gesetzt auf der Gruppe "Favoriten"
constexpr int kDimRole = Qt::UserRole + 2;  // true: gerade nicht anschließbar (abgedunkelt)
constexpr const char* kFavoritesKey = "palette/favorites";
constexpr const char* kColoredKey = "palette/colored";

/// Farbe des Punkts einer Familie; die Farben sind die der Port-Typen im Editor (node_items.cpp).
QColor familyColor(graph::PortFamily family) {
    switch (family) {
        case graph::PortFamily::Image: return portColor(graph::PortType::Field);
        case graph::PortFamily::Value: return portColor(graph::PortType::Scalar);
        case graph::PortFamily::Color: return portColor(graph::PortType::Color);
        case graph::PortFamily::Audio: return portColor(graph::PortType::AudioSpectrum);
        case graph::PortFamily::Other: return QColor{150, 150, 158};
    }
    return QColor{150, 150, 158};
}

QIcon dotIcon(const QColor& color) {
    QPixmap pixmap{12, 12};
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF{2.0, 2.0, 8.0, 8.0});
    return QIcon{pixmap};
}

QStringList loadFavorites() {
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    return settings.value(QString::fromLatin1(kFavoritesKey)).toStringList();
}

/// Baum, dessen Einträge als Kopie auf den Editor gezogen werden. Ein eigenes startDrag verhindert, dass Qt
/// nach einem Verschieben-Drop den Eintrag aus der Palette löscht.
class PaletteTree final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    void startDrag(Qt::DropActions) override {
        QTreeWidgetItem* item = currentItem();
        if (item == nullptr) {
            return;
        }
        const QString typeId = item->data(0, kTypeIdRole).toString();
        const QString libraryName = item->data(0, kLibraryNameRole).toString();
        if (typeId.isEmpty() && libraryName.isEmpty()) {
            return;  // eine Kategorie
        }
        auto* mime = new QMimeData;
        if (typeId.isEmpty()) {
            mime->setData(QString::fromLatin1(kLibraryMime),
                          (item->data(0, kLibraryKindRole).toString() + QLatin1Char('\n') + libraryName).toUtf8());
        } else {
            mime->setData(QString::fromLatin1(kNodeTypeMime), typeId.toUtf8());
        }
        mime->setText(item->text(0));

        QPixmap pixmap{170, 26};
        pixmap.fill(QColor{60, 85, 140});
        {
            QPainter painter{&pixmap};
            painter.setPen(Qt::white);
            painter.drawText(pixmap.rect(), Qt::AlignCenter, item->text(0));
        }
        auto* drag = new QDrag{this};
        drag->setMimeData(mime);
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint{pixmap.width() / 2, pixmap.height() / 2});
        drag->exec(Qt::CopyAction, Qt::CopyAction);
    }
};

/// Kurzer Hinweis für den Tooltip: Ein- und Ausgänge ohne die automatischen Modulationseingänge.
QString describe(const graph::NodeDescriptor& type) {
    QStringList inputs;
    QStringList outputs;
    for (const graph::PortDescriptor& port : type.ports) {
        if (type.findParameter(port.key) != nullptr) {
            continue;  // Modulationseingang eines Parameters
        }
        (port.direction == graph::PortDirection::Input ? inputs : outputs) << fromStd(port.displayName);
    }
    QString text = fromStd(type.displayName) + QStringLiteral("  (") + fromStd(type.typeId) + QLatin1Char(')');
    if (const QString info = nodeInfoText(type.typeId); !info.isEmpty()) {
        text += QStringLiteral("\n\n") + info + QLatin1Char('\n');
    }
    if (!inputs.isEmpty()) {
        text += QStringLiteral("\nEingänge: ") + inputs.join(QStringLiteral(", "));
    }
    if (!outputs.isEmpty()) {
        text += QStringLiteral("\nAusgänge: ") + outputs.join(QStringLiteral(", "));
    }
    text += QStringLiteral("\nParameter: %1").arg(type.parameters.size());
    return text;
}

constexpr const char* kHintNormal = "Auf die Fläche ziehen: Node hinzufügen.\nNode hierher ziehen: entfernen.";
constexpr const char* kHintRemove = "Loslassen: Node wird entfernt";

}  // namespace

NodePalette::NodePalette(std::shared_ptr<const graph::INodeRegistry> registry, QWidget* parent)
    : QWidget{parent}, registry_{std::move(registry)} {
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(4, 4, 4, 4);

    search_ = new QLineEdit{this};
    search_->setPlaceholderText(tr("Nodes suchen …"));
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);

    tree_ = new PaletteTree{this};
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(true);
    tree_->setIndentation(14);
    tree_->setDragEnabled(true);
    tree_->setDragDropMode(QAbstractItemView::DragOnly);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(tree_, 1);

    hint_ = new QLabel{tr(kHintNormal), this};
    hint_->setWordWrap(true);
    layout->addWidget(hint_);
    setRemoveHighlight(false);

    expanded_.insert(tr("Gruppen"));  // die Bibliothek ist klein und soll gleich zu sehen sein
    expanded_.insert(tr("Presets"));
    favorites_ = loadFavorites();
    colored_ = QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}
                   .value(QString::fromLatin1(kColoredKey), true)
                   .toBool();
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &NodePalette::showContextMenu);
    connect(tree_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
        if (!filling_) {
            expanded_.insert(item->text(0));
        }
    });
    connect(tree_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item) {
        if (!filling_) {
            expanded_.remove(item->text(0));
        }
    });
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) { fill(text); });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        const QString typeId = item->data(0, kTypeIdRole).toString();
        if (!typeId.isEmpty()) {
            Q_EMIT addRequested(typeId);
        } else if (const QString name = item->data(0, kLibraryNameRole).toString(); !name.isEmpty()) {
            Q_EMIT libraryAddRequested(item->data(0, kLibraryKindRole).toString(), name);
        }
    });
    fill({});
}

void NodePalette::fill(const QString& filter) {
    filling_ = true;
    tree_->clear();
    const std::vector<const graph::NodeDescriptor*> types = filterNodeTypes(registry_->all(), filter);
    const bool filtered = !filter.trimmed().isEmpty();

    QFont bold = tree_->font();
    bold.setBold(true);
    const auto addType = [&](QTreeWidgetItem* parent, const graph::NodeDescriptor& type, bool star) {
        auto* item = new QTreeWidgetItem{parent};
        item->setText(0, (star ? QStringLiteral("\u2605 ") : QString{}) + fromStd(type.displayName));
        item->setData(0, kTypeIdRole, fromStd(type.typeId));
        item->setToolTip(0, describe(type) + tr("\nRechtsklick: Favorit umschalten"));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    };
    const auto addCategory = [&](const QString& name) {
        auto* category = new QTreeWidgetItem{tree_};
        category->setText(0, name);
        category->setFont(0, bold);
        category->setFlags(Qt::ItemIsEnabled);  // keine Auswahl, kein Ziehen
        // Zugeklappt, außer der Nutzer hat sie geöffnet oder es wird gesucht (dann sollen Treffer sichtbar sein).
        category->setExpanded(filtered || expanded_.contains(name));
        return category;
    };

    // Favoriten oben (nur solche, die der Filter durchlässt).
    QTreeWidgetItem* favoriteGroup = nullptr;
    for (const graph::NodeDescriptor* type : types) {
        if (!favorites_.contains(fromStd(type->typeId))) {
            continue;
        }
        if (favoriteGroup == nullptr) {
            favoriteGroup = addCategory(tr("Favoriten"));
            favoriteGroup->setData(0, kFavoritesRole, true);
            favoriteGroup->setExpanded(true);  // die Schnellzugriffsliste ist kurz und soll sofort da sein
        }
        addType(favoriteGroup, *type, false);
    }

    fillLibrary(filtered, filter, bold);

    QTreeWidgetItem* category = nullptr;
    std::string currentCategory;
    for (const graph::NodeDescriptor* type : types) {
        if (category == nullptr || type->category != currentCategory) {
            currentCategory = type->category;
            category = addCategory(currentCategory.empty() ? tr("Sonstige") : fromStd(currentCategory));
        }
        addType(category, *type, favorites_.contains(fromStd(type->typeId)));
    }
    filling_ = false;
    applyStyles();
    if (filtered && tree_->topLevelItemCount() > 0 && tree_->topLevelItem(0)->childCount() > 0) {
        tree_->setCurrentItem(tree_->topLevelItem(0)->child(0));
    }
}

void NodePalette::applyStyles() {
    const QBrush normal = tree_->palette().brush(QPalette::Text);
    const QBrush dimmed{QColor{110, 110, 118}};
    for (QTreeWidgetItemIterator it{tree_}; *it != nullptr; ++it) {
        QTreeWidgetItem* item = *it;
        const QString typeId = item->data(0, kTypeIdRole).toString();
        if (typeId.isEmpty()) {
            continue;  // eine Kategorie
        }
        const graph::NodeDescriptor* type = registry_->find(typeId.toStdString());
        if (type == nullptr) {
            continue;
        }
        // Punkt: Farbe der Familie des ersten Ausgangs; ohne Ausgang bleibt eine leere Fläche, damit alles bündig steht.
        const std::optional<graph::PortFamily> family = graph::outputFamily(*type);
        item->setIcon(0, colored_ ? (family ? dotIcon(familyColor(*family)) : dotIcon(Qt::transparent)) : QIcon{});
        bool fits = true;
        if (need_) {
            fits = need_->fromOutput ? graph::acceptsInputFrom(*type, need_->type)
                                     : graph::offersOutputFor(*type, need_->type);
        }
        item->setData(0, kDimRole, !fits);
        item->setForeground(0, fits ? normal : dimmed);
    }
}

void NodePalette::setColored(bool on) {
    colored_ = on;
    QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}.setValue(QString::fromLatin1(kColoredKey), on);
    applyStyles();
}

void NodePalette::setWireNeed(graph::PortType type, bool fromOutput) {
    need_ = WireNeed{type, fromOutput};
    applyStyles();
}

void NodePalette::clearWireNeed() {
    need_.reset();
    applyStyles();
}

int NodePalette::dotTypeCount() const {
    int count = 0;
    for (QTreeWidgetItemIterator it{tree_}; *it != nullptr; ++it) {
        const QTreeWidgetItem* item = *it;
        if (!item->data(0, kTypeIdRole).toString().isEmpty() && !item->icon(0).isNull() &&
            !(item->parent() != nullptr && item->parent()->data(0, kFavoritesRole).toBool())) {
            ++count;
        }
    }
    return count;
}

int NodePalette::dimmedTypeCount() const {
    int count = 0;
    for (QTreeWidgetItemIterator it{tree_}; *it != nullptr; ++it) {
        const QTreeWidgetItem* item = *it;
        if (item->data(0, kDimRole).toBool() &&
            !(item->parent() != nullptr && item->parent()->data(0, kFavoritesRole).toBool())) {
            ++count;
        }
    }
    return count;
}

void NodePalette::setFavorite(const QString& typeId, bool on) {
    if (on == favorites_.contains(typeId)) {
        return;
    }
    if (on) {
        favorites_.append(typeId);
    } else {
        favorites_.removeAll(typeId);
    }
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    settings.setValue(QString::fromLatin1(kFavoritesKey), favorites_);
    fill(search_->text());
}

void NodePalette::showContextMenu(const QPoint& position) {
    const QTreeWidgetItem* item = tree_->itemAt(position);
    if (item == nullptr) {
        return;
    }
    if (!item->data(0, kLibraryNameRole).toString().isEmpty()) {
        showLibraryMenu(*item, tree_->viewport()->mapToGlobal(position));
        return;
    }
    const QString typeId = item->data(0, kTypeIdRole).toString();
    if (typeId.isEmpty()) {
        return;  // eine Kategorie
    }
    QMenu menu{this};
    const bool favorite = isFavorite(typeId);
    QAction* toggle = menu.addAction(favorite ? tr("Aus Favoriten entfernen") : tr("Zu Favoriten hinzufügen"));
    if (menu.exec(tree_->viewport()->mapToGlobal(position)) == toggle) {
        setFavorite(typeId, !favorite);
    }
}

int NodePalette::expandedCategoryCount() const {
    int count = 0;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* group = tree_->topLevelItem(i);
        count += (group->isExpanded() && group->data(0, kLibraryKindRole).isNull()) ? 1 : 0;
    }
    return count;
}

void NodePalette::setFilter(const QString& text) { search_->setText(text); }

int NodePalette::visibleTypeCount() const {
    int count = 0;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* group = tree_->topLevelItem(i);
        if (!group->data(0, kFavoritesRole).toBool() && group->data(0, kLibraryKindRole).isNull()) {  // Favoriten: zweite Ansicht; Bibliothek: keine Typen
            count += group->childCount();
        }
    }
    return count;
}

void NodePalette::setRemoveHighlight(bool on) {
    if (on) {
        hint_->setText(tr(kHintRemove));
        hint_->setStyleSheet(QStringLiteral("background:#b03030;color:white;padding:6px;font-weight:bold;"));
        tree_->setStyleSheet(QStringLiteral("QTreeWidget{border:2px solid #b03030;}"));
    } else {
        hint_->setText(tr(kHintNormal));
        hint_->setStyleSheet(QStringLiteral("color:#9a9aa0;padding:2px;"));
        tree_->setStyleSheet(QString{});
    }
}

}  // namespace whitehole::studio
