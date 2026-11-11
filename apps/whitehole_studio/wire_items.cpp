// wire_items.cpp: Zeichnen und Lage der Kabel zwischen zwei Nodes.
//
// Warum es diese Datei gibt: node_items.cpp wurde mit dem Zeichnen der Ports und Anzeigestreifen zu lang. Die Kabel
// brauchen nichts aus dem Inneren der Node-Zeichnung (nur portColor und die Anker der Ports), also stehen sie hier.
// Die Live-Anzeige der Kabel (Glühen) steht in node_items_live.cpp.
#include "node_items.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>
#include <utility>

namespace whitehole::studio {

WireItem::WireItem(ConnectionId id, NodeItem* from, std::string fromKey, NodeItem* to, std::string toKey,
                   graph::PortType type)
    : id_{id}, from_{from}, to_{to}, fromKey_{std::move(fromKey)}, toKey_{std::move(toKey)}, color_{portColor(type)} {
    setFlag(ItemIsSelectable);
    setZValue(-1.0);
    updatePath();
}

void WireItem::updatePath() {
    const PortSlot* a = from_->findPort(fromKey_);
    const PortSlot* b = to_->findPort(toKey_);
    prepareGeometryChange();
    setPath(a != nullptr && b != nullptr ? curve(from_->anchorScene(*a), to_->anchorScene(*b)) : QPainterPath{});
}

QPainterPath WireItem::curve(const QPointF& a, const QPointF& b) {
    QPainterPath path{a};
    const double dx = std::max(40.0, std::abs(b.x() - a.x()) * 0.5);
    path.cubicTo(a + QPointF{dx, 0.0}, b - QPointF{dx, 0.0}, b);
    return path;
}

QPainterPath WireItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(12.0);  // großzügiger als die sichtbare Linie, damit man sie trifft
    return stroker.createStroke(path());
}

QRectF WireItem::boundingRect() const { return shape().boundingRect(); }

void WireItem::setDockHighlight(bool on) {
    if (dockHighlight_ != on) {
        dockHighlight_ = on;
        update();
    }
}

void WireItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    if (dockHighlight_) {
        painter->setPen(QPen{dockColor(), 5.0});
        painter->drawPath(path());
        return;
    }
    if (live_ >= 0.0F && !isSelected()) {
        paintLive(painter);
        return;
    }
    painter->setPen(QPen{isSelected() ? QColor{255, 255, 255} : color_, isSelected() ? 4.0 : 2.5});
    painter->drawPath(path());
}

}  // namespace whitehole::studio
