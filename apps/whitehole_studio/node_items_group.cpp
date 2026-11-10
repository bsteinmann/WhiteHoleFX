// node_items_group.cpp: Die Kachel einer zugeklappten Gruppe (Teil von NodeItem).
//
// Warum eine eigene Datei: node_items.cpp ist an der Größengrenze. Die Kachel ist ein gewöhnliches NodeItem, dessen
// Ports für Ports von Mitgliedern stehen (PortSlot::realNode/realKey); alles andere (Kabel, Treffer, Zeichnen)
// funktioniert darum ohne Sonderfall, nur wo der Graph befragt wird, übersetzt portRef den Port.
#include "node_items.hpp"

#include <QFont>
#include <QPainter>
#include <QPen>

#include <algorithm>

namespace whitehole::studio {

NodeItem::NodeItem(NodeId id, const QString& groupName, std::vector<PortSlot> ports, const QColor& color)
    : id_{id}, title_{groupName}, titleColor_{color}, ports_{std::move(ports)}, groupTile_{true} {
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);
    int inputs = 0;
    int outputs = 0;
    for (PortSlot& slot : ports_) {
        const int row = slot.output ? outputs++ : inputs++;
        slot.anchor = QPointF{slot.output ? kNodeWidth : 0.0, kNodeTitleHeight + 6.0 + row * kNodeRowHeight + kNodeRowHeight / 2.0};
    }
    height_ = kNodeTitleHeight + 12.0 + std::max(1, std::max(inputs, outputs)) * kNodeRowHeight;
}

graph::PortRef NodeItem::portRef(const PortSlot& slot) const {
    if (slot.realNode == 0) {
        return graph::PortRef{id_, slot.key};
    }
    return graph::PortRef{NodeId{slot.realNode}, slot.realKey};
}

GroupFrameItem::GroupFrameItem(QString name, const QColor& color, std::vector<NodeItem*> members)
    : name_{std::move(name)}, color_{color}, members_{std::move(members)} {
    setZValue(-5.0);  // hinter den Nodes und Kabeln
    updateRect();
}

void GroupFrameItem::updateRect() {
    QRectF all;
    for (const NodeItem* member : members_) {
        // boundingRect() der Nodes enthält den Rand für die halben Port-Kreise (12); der Rahmen soll um den Körper liegen.
        all = all.united(member->sceneBoundingRect().adjusted(12.0, 12.0, -12.0, -12.0));
    }
    prepareGeometryChange();
    rect_ = all.adjusted(-18.0, -34.0, 18.0, 18.0);
}

QRectF GroupFrameItem::boundingRect() const { return rect_.adjusted(-2.0, -2.0, 2.0, 2.0); }

void GroupFrameItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    QColor fill = color_;
    fill.setAlpha(30);
    QColor line = color_;
    line.setAlpha(190);
    painter->setPen(QPen{line, 1.5, Qt::DashLine});
    painter->setBrush(fill);
    painter->drawRoundedRect(rect_, 10.0, 10.0);
    QFont bold = painter->font();
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(color_.lighter(160));
    painter->drawText(QRectF{rect_.left() + 12.0, rect_.top(), rect_.width() - 24.0, 28.0}, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(name_, Qt::ElideRight, static_cast<int>(rect_.width() - 24.0)));
}

}  // namespace whitehole::studio
