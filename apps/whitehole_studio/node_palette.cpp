// node_palette.cpp: Baum aller Node-Typen mit Suche und Drag & Drop.
#include "node_palette.hpp"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDrag>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "node_search.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

constexpr int kTypeIdRole = Qt::UserRole;

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
        if (typeId.isEmpty()) {
            return;  // eine Kategorie
        }
        auto* mime = new QMimeData;
        mime->setData(QString::fromLatin1(kNodeTypeMime), typeId.toUtf8());
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

    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) { fill(text); });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        const QString typeId = item->data(0, kTypeIdRole).toString();
        if (!typeId.isEmpty()) {
            Q_EMIT addRequested(typeId);
        }
    });
    fill({});
}

void NodePalette::fill(const QString& filter) {
    tree_->clear();
    const std::vector<const graph::NodeDescriptor*> types = filterNodeTypes(registry_->all(), filter);
    const bool filtered = !filter.trimmed().isEmpty();

    QTreeWidgetItem* category = nullptr;
    std::string currentCategory;
    QFont bold = tree_->font();
    bold.setBold(true);
    for (const graph::NodeDescriptor* type : types) {
        if (category == nullptr || type->category != currentCategory) {
            currentCategory = type->category;
            category = new QTreeWidgetItem{tree_};
            category->setText(0, currentCategory.empty() ? tr("Sonstige") : fromStd(currentCategory));
            category->setFont(0, bold);
            category->setFlags(Qt::ItemIsEnabled);  // keine Auswahl, kein Ziehen
            category->setExpanded(true);
        }
        auto* item = new QTreeWidgetItem{category};
        item->setText(0, fromStd(type->displayName));
        item->setData(0, kTypeIdRole, fromStd(type->typeId));
        item->setToolTip(0, describe(*type));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    }
    if (filtered && tree_->topLevelItemCount() > 0 && tree_->topLevelItem(0)->childCount() > 0) {
        tree_->setCurrentItem(tree_->topLevelItem(0)->child(0));
    }
}

void NodePalette::setFilter(const QString& text) { search_->setText(text); }

int NodePalette::visibleTypeCount() const {
    int count = 0;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        count += tree_->topLevelItem(i)->childCount();
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
