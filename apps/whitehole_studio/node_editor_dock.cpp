// node_editor_dock.cpp: Einrasten eines aus der Palette gezogenen Nodes (Teil von NodeEditor).
//
// Warum eine eigene Datei: Die Regeln, wohin ein Node einrastet, stehen in graph/node_docking.hpp und sind dort
// getestet. Hier steht nur, was Fenster und Maus damit tun: das Ziel unter dem Mauszeiger finden und dem Nutzer
// vor dem Loslassen zeigen, was passieren wird (hellblauer Rahmen, hellblaues Kabel, gestrichelte neue Kabel).
#include <QGraphicsPathItem>
#include <QPen>
#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <limits>

#include "node_editor.hpp"

namespace whitehole::studio {

namespace {

constexpr double kWireReach = 14.0;  // so weit neben einem Kabel rastet ein Node noch ein (Szenenkoordinaten)

/// Kleinster Abstand von `point` zur Kurve (Näherung über Stützpunkte, reicht zum Auswählen).
double distanceToPath(const QPainterPath& path, const QPointF& point) {
    double best = std::numeric_limits<double>::max();
    constexpr int kSamples = 48;
    for (int i = 0; i <= kSamples; ++i) {
        const QPointF at = path.pointAtPercent(static_cast<double>(i) / kSamples);
        best = std::min(best, std::hypot(at.x() - point.x(), at.y() - point.y()));
    }
    return best;
}

}  // namespace

std::optional<graph::DockTarget> NodeEditor::findDockTarget(const QPointF& scenePos) const {
    if (NodeItem* node = nodeAt(scenePos)) {
        if (node->isGroupTile()) {
            return std::nullopt;  // an der Kachel einer Gruppe rastet nichts ein; die Nodes darin sind verdeckt
        }
        graph::DockTarget target;
        target.kind = graph::DockTarget::Kind::Node;
        target.node = node->nodeId();
        const QPointF local = node->mapFromScene(scenePos);
        target.rightHalf = local.x() > node->shape().boundingRect().center().x();
        // Der Port auf Höhe der Maus, auf der gewählten Seite (links Eingänge, rechts Ausgänge)
        double nearest = std::numeric_limits<double>::max();
        for (const PortSlot& port : node->ports()) {
            const double distance = std::abs(port.anchor.y() - local.y());
            if (port.output == target.rightHalf && distance < nearest) {
                nearest = distance;
                target.preferredPort = port.key;
            }
        }
        return target;
    }
    WireItem* closest = nullptr;
    double closestDistance = kWireReach;
    const QRectF probe{scenePos - QPointF{kWireReach, kWireReach}, QSizeF{2 * kWireReach, 2 * kWireReach}};
    for (QGraphicsItem* item : scene_.items(probe, Qt::IntersectsItemShape)) {
        if (auto* wire = dynamic_cast<WireItem*>(item)) {
            const double distance = distanceToPath(wire->path(), scenePos);
            if (distance < closestDistance) {
                closestDistance = distance;
                closest = wire;
            }
        }
    }
    if (closest == nullptr) {
        return std::nullopt;
    }
    graph::DockTarget target;
    target.kind = graph::DockTarget::Kind::Wire;
    target.wire = closest->connectionId();
    return target;
}

std::optional<graph::DockPlan> NodeEditor::dockPlanAt(const std::string& typeId, const QPointF& scenePos, bool alt) const {
    const project::Project* project = player_.project();
    // Der Ausgangs-Node verdrahtet sich beim Anlegen selbst (Player::addNode) und rastet darum nirgends ein.
    if (alt || project == nullptr || typeId == "wh.output.canvas") {
        return std::nullopt;
    }
    const graph::NodeDescriptor* type = project->graph.registry().find(typeId);
    const std::optional<graph::DockTarget> target = findDockTarget(scenePos);
    if (type == nullptr || !target.has_value()) {
        return std::nullopt;
    }
    return graph::planDocking(project->graph, *type, *target);
}

void NodeEditor::showDockPreview(const std::string& typeId, const QPointF& scenePos, bool alt) {
    clearDockPreview();
    const std::optional<graph::DockPlan> plan = dockPlanAt(typeId, scenePos, alt);
    const std::optional<graph::DockTarget> target = plan.has_value() ? findDockTarget(scenePos) : std::nullopt;
    if (!plan.has_value() || !target.has_value()) {
        return;
    }
    if (target->kind == graph::DockTarget::Kind::Wire) {
        for (WireItem* wire : wires_) {
            if (wire->connectionId() == target->wire) {
                wire->setDockHighlight(true);
                dockWire_ = wire;
            }
        }
    } else if (NodeItem* host = itemOf(target->node)) {
        // Der Port des Zielnodes, an dem der neue Node hängen wird
        std::string portKey;
        for (const graph::DockLink& link : plan->links) {
            if (link.to.node == target->node) {
                portKey = link.to.portKey;
            } else if (link.from.node == target->node) {
                portKey = link.from.portKey;
            }
        }
        host->setDockHighlight(true, portKey);
        dockNode_ = host;
    }
    // Gestrichelt: die Kabel, die entstehen. Der neue Node steht dabei unter der Maus.
    const auto anchorOf = [&](const graph::PortRef& ref) -> std::optional<QPointF> {
        if (!ref.node.valid()) {
            return scenePos;
        }
        const NodeItem* item = itemOf(ref.node);
        const PortSlot* port = item != nullptr ? item->findPort(ref.portKey) : nullptr;
        return port != nullptr ? std::optional<QPointF>{item->anchorScene(*port)} : std::nullopt;
    };
    for (const graph::DockLink& link : plan->links) {
        const std::optional<QPointF> a = anchorOf(link.from);
        const std::optional<QPointF> b = anchorOf(link.to);
        if (!a.has_value() || !b.has_value()) {
            continue;
        }
        auto* line = new QGraphicsPathItem{WireItem::curve(*a, *b)};
        line->setPen(QPen{WireItem::dockColor(), 2.0, Qt::DashLine});
        line->setZValue(5.0);
        line->setAcceptedMouseButtons(Qt::NoButton);
        scene_.addItem(line);
        dockLines_.push_back(line);
    }
}

void NodeEditor::clearDockPreview() {
    if (dockWire_ != nullptr) {
        dockWire_->setDockHighlight(false);
        dockWire_ = nullptr;
    }
    if (dockNode_ != nullptr) {
        dockNode_->setDockHighlight(false);
        dockNode_ = nullptr;
    }
    for (QGraphicsPathItem* line : dockLines_) {
        scene_.removeItem(line);
        delete line;
    }
    dockLines_.clear();
}

}  // namespace whitehole::studio
