// node_editor_edit.cpp: Änderungen am Graphen aus dem Node-Editor (Node anlegen, verbinden, löschen,
// verschieben) und das Ziehen von Kabeln.
//
// Warum eine eigene Datei: node_editor.cpp ist sonst über die 400-Zeilen-Grenze. Hier steht alles,
// was den Player ändert; Szene, Maus und Tastatur bleiben in node_editor.cpp.
#include "node_editor.hpp"

#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

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
    const auto targetBefore = player_.project()->target;
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
        if (player_.project()->target != targetBefore) {
            Q_EMIT notice(player_.project()->target
                              ? QStringLiteral("Der gelöschte Node lieferte den Ausgang; der Ausgang sitzt jetzt am Node davor.")
                              : QStringLiteral("Kein Ausgang mehr: Rechtsklick auf einen Field-Ausgang, \"Als Ausgang setzen\"."));
        }
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

}  // namespace whitehole::studio
