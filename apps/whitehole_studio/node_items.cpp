// node_items.cpp: Zeichnen von Nodes und Kabeln.
#include "node_items.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace whitehole::studio {

namespace {

constexpr double kWidth = kNodeWidth;
constexpr double kTitleHeight = kNodeTitleHeight;
constexpr double kRowHeight = kNodeRowHeight;
constexpr double kPortRadius = 5.0;
constexpr double kPortHit = 10.0;  // halbe Kantenlänge der Klickfläche
constexpr double kMargin = 12.0;   // Platz um den Node für die halben Port-Kreise

}  // namespace

QColor portColor(graph::PortType type) {
    switch (type) {
        case graph::PortType::Field: return {64, 140, 255};         // Blau
        case graph::PortType::Image: return {178, 92, 255};         // Violett
        case graph::PortType::Scalar: return {92, 214, 70};         // Grün
        case graph::PortType::Integer: return {0, 206, 168};        // Türkis
        case graph::PortType::Boolean: return {255, 214, 40};       // Gelb
        case graph::PortType::Color: return {255, 100, 190};        // Pink
        case graph::PortType::Palette: return {255, 144, 40};       // Orange
        case graph::PortType::Text: return {226, 226, 226};         // Weiß
        case graph::PortType::Trigger: return {255, 70, 70};        // Rot
        case graph::PortType::AudioSpectrum: return {0, 190, 255};  // Himmelblau
        case graph::PortType::PointCloud: return {176, 140, 96};    // Sand
        case graph::PortType::AudioWave: return {176, 255, 120};    // Hellgrün
    }
    return {200, 200, 200};
}

PortShape portShape(graph::PortType type) {
    switch (type) {
        case graph::PortType::Integer:
        case graph::PortType::Boolean:
        case graph::PortType::Trigger: return PortShape::Diamond;
        case graph::PortType::Text:
        case graph::PortType::PointCloud: return PortShape::Square;
        case graph::PortType::AudioSpectrum:
        case graph::PortType::AudioWave: return PortShape::Triangle;
        case graph::PortType::Field:
        case graph::PortType::Image:
        case graph::PortType::Scalar:
        case graph::PortType::Color:
        case graph::PortType::Palette: return PortShape::Circle;
    }
    return PortShape::Circle;
}

void drawPortMarker(QPainter* painter, const QPointF& center, graph::PortType type, bool output, double radius) {
    const QColor color = portColor(type);
    QPainterPath path;
    switch (portShape(type)) {
        case PortShape::Circle: path.addEllipse(center, radius, radius); break;
        case PortShape::Square:
            path.addRect(QRectF{center.x() - radius * 0.9, center.y() - radius * 0.9, radius * 1.8, radius * 1.8});
            break;
        case PortShape::Diamond: {
            const double d = radius * 1.3;
            QPolygonF diamond;
            diamond << center + QPointF{0.0, -d} << center + QPointF{d, 0.0} << center + QPointF{0.0, d}
                    << center + QPointF{-d, 0.0};
            path.addPolygon(diamond);
            path.closeSubpath();
            break;
        }
        case PortShape::Triangle: {
            const double d = radius * 1.25;
            QPolygonF triangle;
            triangle << center + QPointF{-d, -d} << center + QPointF{d, 0.0} << center + QPointF{-d, d};
            path.addPolygon(triangle);
            path.closeSubpath();
            break;
        }
    }
    if (output) {
        painter->setPen(QPen{QColor{20, 20, 24}, 1.0});
        painter->setBrush(color);
    } else {
        painter->setPen(QPen{color, 2.2});
        painter->setBrush(QColor{30, 31, 36});
    }
    painter->drawPath(path);
}

QString checkPortStyles() {
    using graph::PortType;
    const PortType all[] = {PortType::Field,   PortType::Image,         PortType::Scalar,      PortType::Integer,
                            PortType::Boolean, PortType::Color,         PortType::Palette,     PortType::Text,
                            PortType::Trigger, PortType::AudioSpectrum, PortType::PointCloud, PortType::AudioWave};
    for (std::size_t i = 0; i < std::size(all); ++i) {
        for (std::size_t j = i + 1; j < std::size(all); ++j) {
            const QColor a = portColor(all[i]);
            const QColor b = portColor(all[j]);
            const int distance =
                std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) + std::abs(a.blue() - b.blue());
            // Gleiche Form verlangt deutlich verschiedene Farben; verschiedene Formen genügen schwächere Unterschiede.
            const int needed = portShape(all[i]) == portShape(all[j]) ? 150 : 60;
            if (distance < needed) {
                return QStringLiteral("Port-Typen %1 und %2 sind zu ähnlich (Abstand %3)")
                    .arg(static_cast<int>(all[i]))
                    .arg(static_cast<int>(all[j]))
                    .arg(distance);
            }
        }
    }
    return {};
}

NodeItem::NodeItem(NodeId id, const graph::NodeDescriptor& descriptor, const QString& title)
    : id_{id}, title_{title}, titleColor_{70, 74, 84} {
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);  // für den Tooltip am Info-Zeichen
    canBypass_ = descriptor.domain == graph::NodeDomain::Field;

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
    height_ = sizeFor(descriptor).height();
}

QSizeF NodeItem::size() const { return QSizeF{kWidth, height_}; }

QSizeF NodeItem::sizeFor(const graph::NodeDescriptor& descriptor) {
    int inputs = 0;
    int outputs = 0;
    for (const graph::PortDescriptor& port : descriptor.ports) {
        (port.direction == graph::PortDirection::Output ? outputs : inputs) += 1;
    }
    return QSizeF{kWidth, kTitleHeight + 12.0 + std::max(1, std::max(inputs, outputs)) * kRowHeight};
}

namespace {
// Der Schalter sitzt rechts im Kopf, ein Kreis mit Strich ("Ein/Aus"-Zeichen).
const QRectF kBypassButton{kWidth - 24.0, (kTitleHeight - 16.0) / 2.0, 16.0, 16.0};

/// Das Info-Zeichen sitzt links neben dem Schalter (oder, ohne Schalter, ganz rechts).
QRectF infoRect(bool hasBypass) {
    return QRectF{kWidth - (hasBypass ? 46.0 : 24.0), (kTitleHeight - 16.0) / 2.0, 16.0, 16.0};
}
}  // namespace

void NodeItem::setInfo(const QString& text) {
    info_ = text;
    update();
}

bool NodeItem::infoButtonAt(const QPointF& scenePos) const {
    return !info_.isEmpty() && infoRect(canBypass_).adjusted(-3.0, -3.0, 3.0, 3.0).contains(mapFromScene(scenePos));
}

void NodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
    const bool over = !info_.isEmpty() && infoRect(canBypass_).adjusted(-3.0, -3.0, 3.0, 3.0).contains(event->pos());
    setToolTip(over ? title_ + QStringLiteral("\n\n") + info_ : QString{});
    QGraphicsItem::hoverMoveEvent(event);
}

void NodeItem::setBypassed(bool on) {
    if (bypassed_ != on) {
        bypassed_ = on;
        update();
    }
}

bool NodeItem::bypassButtonAt(const QPointF& scenePos) const {
    return canBypass_ && kBypassButton.adjusted(-3.0, -3.0, 3.0, 3.0).contains(mapFromScene(scenePos));
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

void NodeItem::setDockHighlight(bool on, std::string portKey) {
    if (dockHighlight_ == on && dockPort_ == portKey) {
        return;
    }
    dockHighlight_ = on;
    dockPort_ = on ? std::move(portKey) : std::string{};
    update();
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
    if (bypassed_) {
        painter->setOpacity(0.5);
    }
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
    if (dockHighlight_) {
        border = WireItem::dockColor();
        borderWidth = 3.0;
    }
    painter->setPen(QPen{border, borderWidth});
    painter->setBrush(QColor{44, 46, 54});
    painter->drawRoundedRect(body, 6.0, 6.0);
    if (groupTile_) {
        // Doppelter Rand: erkennbar als Gruppe, die mehr enthält, als man sieht.
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen{QColor{150, 154, 170}, 1.0});
        painter->drawRoundedRect(body.adjusted(3.0, 3.0, -3.0, -3.0), 4.0, 4.0);
    }

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
    const double titleWidth = kWidth - (canBypass_ ? 44.0 : 20.0) - (info_.isEmpty() ? 0.0 : 22.0);
    painter->drawText(QRectF{10.0, 0.0, titleWidth, kTitleHeight}, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(title_, Qt::ElideRight, static_cast<int>(titleWidth)));
    if (!info_.isEmpty()) {
        // "i" im Kreis
        const QRectF box = infoRect(canBypass_);
        painter->save();
        painter->setOpacity(1.0);
        painter->setPen(QPen{QColor{150, 190, 255}, 1.5});
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(box.adjusted(1.0, 1.0, -1.0, -1.0));
        painter->setPen(QPen{QColor{150, 190, 255}, 1.8, Qt::SolidLine, Qt::RoundCap});
        painter->drawPoint(QPointF{box.center().x(), box.top() + 4.6});
        painter->drawLine(QPointF{box.center().x(), box.top() + 7.2}, QPointF{box.center().x(), box.bottom() - 3.8});
        painter->restore();
    }
    if (canBypass_) {
        const QPointF center = kBypassButton.center();
        const QColor mark = bypassed_ ? QColor{255, 110, 100} : QColor{130, 230, 150};
        painter->save();
        painter->setOpacity(1.0);
        painter->setPen(QPen{mark, 1.8, Qt::SolidLine, Qt::RoundCap});
        painter->setBrush(Qt::NoBrush);
        painter->drawArc(QRectF{center.x() - 5.5, center.y() - 5.5, 11.0, 11.0}, 120 * 16, 300 * 16);  // Lücke oben
        painter->drawLine(QPointF{center.x(), center.y() - 6.5}, QPointF{center.x(), center.y() - 0.5});
        painter->restore();
    }
    if (bypassed_) {
        // Durchleitung: vom ersten Field-Eingang zum ersten Ausgang, gestrichelt quer durch den Node.
        const PortSlot* in = nullptr;
        const PortSlot* out = nullptr;
        for (const PortSlot& port : ports_) {
            if (!port.output && in == nullptr && port.type == graph::PortType::Field) {
                in = &port;
            } else if (port.output && out == nullptr) {
                out = &port;
            }
        }
        if (in != nullptr && out != nullptr) {
            painter->setPen(QPen{QColor{200, 205, 215}, 1.5, Qt::DashLine});
            painter->drawLine(in->anchor, out->anchor);
        }
    }
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
        if (dockHighlight_ && !dockPort_.empty() && port.key == dockPort_) {
            painter->setPen(QPen{WireItem::dockColor(), 2.5});
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(port.anchor, kPortRadius + 4.5, kPortRadius + 4.5);
        }
        drawPortMarker(painter, port.anchor, port.type, port.output, kPortRadius);

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
    painter->setPen(QPen{isSelected() ? QColor{255, 255, 255} : color_, isSelected() ? 4.0 : 2.5});
    painter->drawPath(path());
}

}  // namespace whitehole::studio
