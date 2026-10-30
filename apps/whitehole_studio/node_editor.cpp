// node_editor.cpp: Szene aufbauen, Maus und Tastatur.
#include "node_editor.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

NodeEditor::NodeEditor(runtime::Player& player, QWidget* parent) : QGraphicsView{parent}, player_{player} {
    setScene(&scene_);
    setRenderHint(QPainter::Antialiasing);
    setBackgroundBrush(QColor{30, 31, 36});
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFocusPolicy(Qt::StrongFocus);
}

void NodeEditor::reset() {
    rebuild();
    fitPending_ = true;
    showAll();
}

void NodeEditor::rebuild() {
    // Erst die Zeiger vergessen, dann die Szene leeren: clear() löscht alle Items.
    wires_.clear();
    nodes_.clear();
    tempWire_ = nullptr;
    wireNode_ = nullptr;
    scene_.clear();

    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    const graph::Graph& g = project->graph;

    std::unordered_map<std::string, int> total;
    for (const graph::NodeInstance& node : g.nodes()) {
        ++total[node.typeId];
    }
    std::unordered_map<std::string, int> seen;
    for (const graph::NodeInstance& node : g.nodes()) {
        const int index = ++seen[node.typeId];
        const graph::NodeDescriptor* descriptor = g.registry().find(node.typeId);
        if (descriptor == nullptr) {
            continue;
        }
        // Dieselbe Nummerierung wie im Dock "Parameter" (Player::nodes).
        QString title = fromStd(descriptor->displayName);
        if (total[node.typeId] > 1) {
            title += QStringLiteral(" %1").arg(index);
        }
        auto* item = new NodeItem{node.id, *descriptor, title};
        item->setPos(static_cast<qreal>(node.position.x), static_cast<qreal>(node.position.y));
        scene_.addItem(item);
        item->setMoveCallback([this](NodeItem& moved) {
            for (WireItem* wire : wires_) {
                if (wire->fromNode() == &moved || wire->toNode() == &moved) {
                    wire->updatePath();
                }
            }
        });
        nodes_.emplace(node.id.value(), item);
    }
    if (project->target) {
        if (NodeItem* item = itemOf(project->target->node)) {
            item->setTargetKey(project->target->portKey);
        }
    }
    for (const graph::Connection& c : g.connections()) {
        NodeItem* from = itemOf(c.from.node);
        NodeItem* to = itemOf(c.to.node);
        const PortSlot* port = from != nullptr ? from->findPort(c.from.portKey) : nullptr;
        if (from == nullptr || to == nullptr || port == nullptr) {
            continue;
        }
        auto* wire = new WireItem{c.id, from, c.from.portKey, to, c.to.portKey, port->type};
        scene_.addItem(wire);
        wires_.push_back(wire);
    }
    const QRectF content = scene_.itemsBoundingRect();
    scene_.setSceneRect(content.adjusted(-1500, -1500, 1500, 1500));
}

void NodeEditor::showAll() {
    QRectF content;
    for (const auto& [id, item] : nodes_) {
        content = content.united(item->sceneBoundingRect());
    }
    resetTransform();
    if (content.isNull()) {
        return;
    }
    content.adjust(-40, -40, 40, 40);
    fitInView(content, Qt::KeepAspectRatio);
    if (transform().m11() > 1.0) {
        resetTransform();
        centerOn(content.center());
    }
}

NodeItem* NodeEditor::itemOf(NodeId id) const {
    const auto it = nodes_.find(id.value());
    return it == nodes_.end() ? nullptr : it->second;
}

NodeEditor::Hit NodeEditor::portAt(const QPointF& scenePos) const {
    for (QGraphicsItem* item : scene_.items(scenePos)) {
        if (auto* node = dynamic_cast<NodeItem*>(item)) {
            if (const PortSlot* port = node->portAt(scenePos)) {
                return {node, port};
            }
        }
    }
    return {};
}

NodeItem* NodeEditor::nodeAt(const QPointF& scenePos) const {
    for (QGraphicsItem* item : scene_.items(scenePos)) {
        if (auto* node = dynamic_cast<NodeItem*>(item)) {
            return node;
        }
    }
    return nullptr;
}

WireItem* NodeEditor::wireAt(const QPointF& scenePos) const {
    for (QGraphicsItem* item : scene_.items(scenePos)) {
        if (auto* wire = dynamic_cast<WireItem*>(item)) {
            return wire;
        }
    }
    return nullptr;
}

void NodeEditor::fail(const std::string& message) { Q_EMIT failed(fromStd(message)); }

// --- Änderungen am Graphen ----------------------------------------------------------------------

std::optional<NodeId> NodeEditor::addNodeAt(const std::string& typeId, const QPointF& scenePos) {
    const auto id = player_.addNode(typeId, Vec2{static_cast<float>(scenePos.x()), static_cast<float>(scenePos.y())});
    if (!id.ok()) {
        fail(id.error().message);
        return std::nullopt;
    }
    rebuild();
    if (NodeItem* item = itemOf(id.value())) {
        item->setSelected(true);
    }
    Q_EMIT edited(true);
    return id.value();
}

bool NodeEditor::connectPorts(const graph::PortRef& from, const graph::PortRef& to) {
    const auto id = player_.connect(from, to);
    if (!id.ok()) {
        fail(id.error().message);
        return false;
    }
    rebuild();
    Q_EMIT edited(true);
    return true;
}

bool NodeEditor::setTarget(const graph::PortRef& port) {
    const auto r = player_.setTarget(port);
    if (!r.ok()) {
        fail(r.error().message);
        return false;
    }
    rebuild();
    Q_EMIT edited(true);
    return true;
}

void NodeEditor::removeSelected() {
    std::vector<ConnectionId> connections;
    std::vector<NodeId> nodes;
    for (QGraphicsItem* item : scene_.selectedItems()) {
        if (auto* wire = dynamic_cast<WireItem*>(item)) {
            connections.push_back(wire->connectionId());
        } else if (auto* node = dynamic_cast<NodeItem*>(item)) {
            nodes.push_back(node->nodeId());
        }
    }
    bool changed = false;
    std::string error;
    for (const ConnectionId id : connections) {
        changed = player_.disconnect(id).ok() || changed;
    }
    for (const NodeId id : nodes) {
        const auto r = player_.removeNode(id);
        if (r.ok()) {
            changed = true;
        } else {
            error = r.error().message;
        }
    }
    if (changed) {
        rebuild();
        Q_EMIT edited(true);
    }
    if (!error.empty()) {
        fail(error);
    }
}

void NodeEditor::commitMoves() {
    bool moved = false;
    for (const auto& [id, item] : nodes_) {
        if (item->takeMoved()) {
            moved = player_.moveNode(item->nodeId(), Vec2{static_cast<float>(item->pos().x()),
                                                           static_cast<float>(item->pos().y())})
                        .ok() ||
                    moved;
        }
    }
    if (moved) {
        Q_EMIT edited(false);
    }
}

// --- Kabel ziehen -------------------------------------------------------------------------------

bool NodeEditor::beginWire(const QPointF& scenePos) {
    const Hit hit = portAt(scenePos);
    if (hit.node == nullptr) {
        return false;
    }
    wireNode_ = hit.node;
    wireSlot_ = *hit.port;
    tempWire_ = scene_.addPath(QPainterPath{}, QPen{portColor(wireSlot_.type), 2.5, Qt::DashLine});
    tempWire_->setZValue(10.0);
    moveWire(scenePos);
    return true;
}

void NodeEditor::moveWire(const QPointF& scenePos) {
    if (tempWire_ == nullptr || wireNode_ == nullptr) {
        return;
    }
    const QPointF anchor = wireNode_->anchorScene(wireSlot_);
    tempWire_->setPath(wireSlot_.output ? WireItem::curve(anchor, scenePos) : WireItem::curve(scenePos, anchor));
}

void NodeEditor::cancelWire() {
    if (tempWire_ != nullptr) {
        scene_.removeItem(tempWire_);
        delete tempWire_;
        tempWire_ = nullptr;
    }
    wireNode_ = nullptr;
}

bool NodeEditor::endWire(const QPointF& scenePos) {
    if (wireNode_ == nullptr) {
        return false;
    }
    NodeItem* startNode = wireNode_;
    const PortSlot start = wireSlot_;
    cancelWire();

    const Hit hit = portAt(scenePos);
    if (hit.node == nullptr || hit.node == startNode || hit.port->output == start.output) {
        return false;  // ins Leere oder auf einen unpassenden Port: abgebrochen, keine Meldung
    }
    const graph::PortRef a{startNode->nodeId(), start.key};
    const graph::PortRef b{hit.node->nodeId(), hit.port->key};
    return start.output ? connectPorts(a, b) : connectPorts(b, a);
}

// --- Maus, Tastatur -----------------------------------------------------------------------------

void NodeEditor::mousePressEvent(QMouseEvent* event) {
    fitPending_ = false;
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        panLast_ = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && beginWire(mapToScene(event->position().toPoint()))) {
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void NodeEditor::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint now = event->position().toPoint();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (now.x() - panLast_.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() - (now.y() - panLast_.y()));
        panLast_ = now;
        event->accept();
        return;
    }
    if (wireNode_ != nullptr) {
        moveWire(mapToScene(event->position().toPoint()));
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void NodeEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_ && event->button() == Qt::MiddleButton) {
        panning_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    if (wireNode_ != nullptr && event->button() == Qt::LeftButton) {
        endWire(mapToScene(event->position().toPoint()));
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    commitMoves();
}

void NodeEditor::wheelEvent(QWheelEvent* event) {
    fitPending_ = false;
    const double factor = std::pow(1.0015, event->angleDelta().y());
    const double next = transform().m11() * factor;
    if (next >= 0.25 && next <= 3.0) {
        scale(factor, factor);
    }
    event->accept();
}

void NodeEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        removeSelected();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F && event->modifiers() == Qt::NoModifier) {
        showAll();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void NodeEditor::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (fitPending_) {
        showAll();  // solange niemand die Ansicht angefasst hat, folgt sie der Fenstergröße
    }
}

// --- Rauchtest ----------------------------------------------------------------------------------

QString NodeEditor::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("kein Projekt");
    }
    const std::size_t nodesBefore = player_.project()->graph.nodes().size();
    const std::size_t wiresBefore = player_.project()->graph.connections().size();

    const auto freeSpot = [this]() {
        const QRectF content = scene_.itemsBoundingRect();
        return QPointF{content.left() + 20.0, content.bottom() + 80.0};
    };
    const std::optional<NodeId> source = addNodeAt("wh.generator.plasma", freeSpot());
    if (!source) {
        return QStringLiteral("Plasma-Node anlegen");
    }
    const std::optional<NodeId> sink = addNodeAt("wh.transform.mirror", freeSpot());
    if (!sink) {
        return QStringLiteral("Mirror-Node anlegen");
    }
    NodeItem* from = itemOf(*source);
    NodeItem* to = itemOf(*sink);
    if (from == nullptr || to == nullptr || from->findPort("out") == nullptr || to->findPort("source") == nullptr) {
        return QStringLiteral("Ports der neuen Nodes nicht gefunden");
    }

    // Wie mit der Maus: am Ausgang anfassen, über den Eingang ziehen, loslassen.
    const QPointF start = from->anchorScene(*from->findPort("out"));
    const QPointF end = to->anchorScene(*to->findPort("source"));
    if (!beginWire(start)) {
        return QStringLiteral("Ausgang bei %1/%2 nicht getroffen").arg(start.x()).arg(start.y());
    }
    moveWire((start + end) / 2.0);
    moveWire(end);
    const Hit probe = portAt(end);
    const QString probeNote = QStringLiteral("Treffer am Eingang: Node %1, Port %2, erwartet Node %3, Items an der Stelle %4")
                                  .arg(probe.node != nullptr ? static_cast<qulonglong>(probe.node->nodeId().value()) : 0ULL)
                                  .arg(probe.port != nullptr ? QString::fromStdString(probe.port->key) : QStringLiteral("-"))
                                  .arg(static_cast<qulonglong>(sink->value()))
                                  .arg(scene_.items(end).size());
    const bool connected = endWire(end);
    if (!connected || player_.project()->graph.connections().size() != wiresBefore + 1 ||
        wires_.size() != wiresBefore + 1) {
        return QStringLiteral("Kabel ziehen: endWire=%1, Verbindungen %2, Kabel %3, erwartet %4 (Eingang bei %5/%6); %7")
            .arg(connected)
            .arg(player_.project()->graph.connections().size())
            .arg(wires_.size())
            .arg(wiresBefore + 1)
            .arg(end.x())
            .arg(end.y())
            .arg(probeNote);
    }

    // Kabel auswählen und löschen
    for (WireItem* wire : wires_) {
        const auto* c = player_.project()->graph.findConnection(wire->connectionId());
        if (c != nullptr && c->from.node == *source) {
            wire->setSelected(true);
        }
    }
    removeSelected();
    if (player_.project()->graph.connections().size() != wiresBefore) {
        return QStringLiteral("Kabel löschen: %1 Verbindungen, erwartet %2").arg(player_.project()->graph.connections().size()).arg(wiresBefore);
    }

    // Verschieben wie mit der Maus: Position ändern, Änderung übernehmen
    from = itemOf(*source);
    from->setPos(from->pos() + QPointF{30.0, 10.0});
    commitMoves();

    // Beide Nodes auswählen und löschen
    itemOf(*source)->setSelected(true);
    itemOf(*sink)->setSelected(true);
    removeSelected();
    if (player_.project()->graph.nodes().size() != nodesBefore || nodes_.size() != nodesBefore) {
        return QStringLiteral("Aufräumen: %1 Nodes im Graph, %2 in der Szene, erwartet %3")
            .arg(player_.project()->graph.nodes().size())
            .arg(nodes_.size())
            .arg(nodesBefore);
    }
    return {};
}

}  // namespace whitehole::studio
