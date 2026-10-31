// node_items.cpp: Zeichnen von Nodes und Kabeln.
#include "node_items.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

namespace {

constexpr double kWidth = 190.0;
constexpr double kTitleHeight = 26.0;
constexpr double kRowHeight = 22.0;
constexpr double kPortRadius = 5.0;
constexpr double kPortHit = 10.0;  // halbe Kantenlänge der Klickfläche
constexpr double kMargin = 12.0;   // Platz um den Node für die halben Port-Kreise

}  // namespace

QColor portColor(graph::PortType type) {
    switch (type) {
        case graph::PortType::Field: return {100, 170, 255};
        case graph::PortType::Image: return {180, 120, 255};
        case graph::PortType::Scalar: return {120, 210, 120};
        case graph::PortType::Integer: return {90, 200, 170};
        case graph::PortType::Boolean: return {240, 200, 80};
        case graph::PortType::Color: return {255, 130, 170};
        case graph::PortType::Palette: return {255, 160, 90};
        case graph::PortType::Text: return {200, 200, 200};
        case graph::PortType::Trigger: return {255, 100, 100};
        case graph::PortType::AudioSpectrum: return {90, 220, 230};
        case graph::PortType::PointCloud: return {160, 160, 255};
        case graph::PortType::AudioWave: return {130, 230, 150};
    }
    return {200, 200, 200};
}

NodeItem::NodeItem(NodeId id, const graph::NodeDescriptor& descriptor, const QString& title)
    : id_{id}, title_{title}, titleColor_{70, 74, 84} {
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);

    int inputs = 0;
    int outputs = 0;
    for (const graph::PortDescriptor& port : descriptor.ports) {
        PortSlot slot;
        slot.key = port.key;
        slot.label = QString::fromUtf8(port.displayName.data(), static_cast<qsizetype>(port.displayName.size()));
        slot.type = port.type;
        slot.output = port.direction == graph::PortDirection::Output;
        const int row = slot.output ? outputs++ : inputs++;
        slot.anchor = QPointF{slot.output ? kWidth : 0.0, kTitleHeight + 6.0 + row * kRowHeight + kRowHeight / 2.0};
        if (slot.output && titleColor_ == QColor{70, 74, 84}) {
            titleColor_ = portColor(slot.type).darker(220);
        }
        ports_.push_back(std::move(slot));
    }
    height_ = kTitleHeight + 12.0 + std::max(1, std::max(inputs, outputs)) * kRowHeight;
}

const PortSlot* NodeItem::findPort(const std::string& key) const {
    const auto it = std::ranges::find(ports_, key, &PortSlot::key);
    return it == ports_.end() ? nullptr : &*it;
}

const PortSlot* NodeItem::portAt(const QPointF& scenePos) const {
    const QPointF local = mapFromScene(scenePos);
    for (const PortSlot& port : ports_) {
        if (QRectF{port.anchor.x() - kPortHit, port.anchor.y() - kPortHit, 2 * kPortHit, 2 * kPortHit}.contains(local)) {
            return &port;
        }
    }
    return nullptr;
}

void NodeItem::setTargetKey(std::optional<std::string> key) {
    targetKey_ = std::move(key);
    update();
}

void NodeItem::setMoveCallback(std::function<void(NodeItem&)> callback) {
    onMoved_ = std::move(callback);
    setFlag(ItemSendsGeometryChanges);
}

QRectF NodeItem::boundingRect() const {
    return QRectF{-kMargin, -kMargin, kWidth + 2 * kMargin, height_ + 2 * kMargin};
}

QPainterPath NodeItem::shape() const {
    QPainterPath path;
    // Winding statt Even-Odd: Die Port-Flächen überlappen den Körper, und bei Even-Odd wäre der
    // Überlappungsbereich ein Loch, in dem man den Port nicht treffen würde.
    path.setFillRule(Qt::WindingFill);
    path.addRoundedRect(QRectF{0.0, 0.0, kWidth, height_}, 6.0, 6.0);
    for (const PortSlot& port : ports_) {
        path.addRect(QRectF{port.anchor.x() - kPortHit, port.anchor.y() - kPortHit, 2 * kPortHit, 2 * kPortHit});
    }
    return path;
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        moved_ = true;
        if (onMoved_) {
            onMoved_(*this);
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF body{0.0, 0.0, kWidth, height_};
    const bool isTarget = targetKey_.has_value();

    QColor border{96, 100, 112};
    double borderWidth = 1.0;
    if (isTarget) {
        border = QColor{255, 170, 60};
        borderWidth = 2.0;
    }
    if (isSelected()) {
        border = QColor{255, 255, 255};
        borderWidth = 2.0;
    }
    painter->setPen(QPen{border, borderWidth});
    painter->setBrush(QColor{44, 46, 54});
    painter->drawRoundedRect(body, 6.0, 6.0);

    painter->save();
    painter->setClipRect(QRectF{0.0, 0.0, kWidth, kTitleHeight});
    painter->setPen(Qt::NoPen);
    painter->setBrush(titleColor_);
    painter->drawRoundedRect(body, 6.0, 6.0);
    painter->restore();

    QFont bold = painter->font();
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(QColor{240, 240, 245});
    painter->drawText(QRectF{10.0, 0.0, kWidth - 20.0, kTitleHeight}, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(title_, Qt::ElideRight, static_cast<int>(kWidth - 20.0)));
    bold.setBold(false);
    painter->setFont(bold);

    const int labelWidth = static_cast<int>(kWidth / 2.0 - 16.0);
    for (const PortSlot& port : ports_) {
        const bool target = port.output && targetKey_ && *targetKey_ == port.key;
        if (target) {
            painter->setPen(QPen{QColor{255, 170, 60}, 2.0});
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(port.anchor, kPortRadius + 3.5, kPortRadius + 3.5);
        }
        painter->setPen(QPen{QColor{20, 20, 24}, 1.0});
        painter->setBrush(portColor(port.type));
        painter->drawEllipse(port.anchor, kPortRadius, kPortRadius);

        painter->setPen(QColor{205, 207, 214});
        const QString text = painter->fontMetrics().elidedText(port.label, Qt::ElideRight, labelWidth);
        if (port.output) {
            painter->drawText(QRectF{port.anchor.x() - 10.0 - labelWidth, port.anchor.y() - kRowHeight / 2.0,
                                     static_cast<double>(labelWidth), kRowHeight},
                              Qt::AlignVCenter | Qt::AlignRight, text);
        } else {
            painter->drawText(QRectF{port.anchor.x() + 10.0, port.anchor.y() - kRowHeight / 2.0,
                                     static_cast<double>(labelWidth), kRowHeight},
                              Qt::AlignVCenter | Qt::AlignLeft, text);
        }
    }
}

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

void WireItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen{isSelected() ? QColor{255, 255, 255} : color_, isSelected() ? 4.0 : 2.5});
    painter->drawPath(path());
}

}  // namespace whitehole::studio
