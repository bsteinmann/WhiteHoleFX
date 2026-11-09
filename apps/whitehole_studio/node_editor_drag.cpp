// node_editor_drag.cpp: Drag & Drop zwischen Node-Palette und Editor (Teil von NodeEditor).
//
// Hinzufügen: Die Palette startet einen Qt-Drag mit dem Typ als MIME-Daten; der Editor legt beim
// Loslassen den Node an der Stelle an.
// Entfernen: Das Verschieben von Nodes ist kein Qt-Drag, sondern Bewegen von Szene-Items. Darum prüft
// der Editor beim Loslassen der Maus selbst, ob sie über der "Löschzone" (der Palette) steht.
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "node_editor.hpp"
#include "node_palette.hpp"

namespace whitehole::studio {

namespace {

bool carriesNodeType(const QMimeData* mime) { return mime != nullptr && mime->hasFormat(QString::fromLatin1(kNodeTypeMime)); }

std::string nodeTypeOf(const QMimeData* mime) {
    return QString::fromUtf8(mime->data(QString::fromLatin1(kNodeTypeMime))).toStdString();
}

/// Alt gedrückt? Während eines Drags bekommt das Fenster keine Tastenereignisse, darum der Blick auf die Tastatur.
bool altPressed() { return QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier); }

}  // namespace

void NodeEditor::dragEnterEvent(QDragEnterEvent* event) {
    if (carriesNodeType(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void NodeEditor::dragMoveEvent(QDragMoveEvent* event) {
    if (carriesNodeType(event->mimeData())) {
        event->acceptProposedAction();
        showDockPreview(nodeTypeOf(event->mimeData()), mapToScene(event->position().toPoint()), altPressed());
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void NodeEditor::dragLeaveEvent(QDragLeaveEvent* event) {
    clearDockPreview();  // Esc oder aus dem Editor gezogen
    QGraphicsView::dragLeaveEvent(event);
}

void NodeEditor::dropEvent(QDropEvent* event) {
    if (!handleNodeDrop(event->mimeData(), event->position().toPoint())) {
        QGraphicsView::dropEvent(event);
        return;
    }
    event->acceptProposedAction();
    // Der Drag läuft unter Windows in einer eigenen Schleife des Systems; danach das Bild sicher neu zeichnen lassen.
    // Dazu noch einmal prüfen, ob Nodes im Bild sind: Fenster dürfen sich nach dem Ablegen noch umbauen.
    QTimer::singleShot(0, this, [this]() {
        ensureNodesVisible();
        scene_.update();
        viewport()->update();
    });
}

bool NodeEditor::handleNodeDrop(const QMimeData* mime, const QPoint& viewportPos) {
    if (!carriesNodeType(mime)) {
        return false;
    }
    const std::string typeId = nodeTypeOf(mime);
    fitPending_ = false;  // der Nutzer arbeitet im Editor: die Ansicht folgt der Fenstergröße nicht mehr von selbst
    // Abgelegt wird nur, was im Editor liegt; eine Stelle daneben (Fehler im Drag-System) landet am Rand.
    const QRect area = viewport()->rect();
    const QPoint inside{std::clamp(viewportPos.x(), 0, std::max(0, area.width() - 1)),
                        std::clamp(viewportPos.y(), 0, std::max(0, area.height() - 1))};
    const QPointF scenePos = mapToScene(inside);
    const std::optional<graph::DockPlan> plan = dockPlanAt(typeId, scenePos, altPressed());
    clearDockPreview();
    // Der Mauszeiger steht in der Mitte des neuen Nodes; eingerastet sitzt er zwischen seinen Nachbarn (Platz machen)
    const std::optional<graph::DockTarget> target = plan.has_value() ? findDockTarget(scenePos) : std::nullopt;
    const graph::NodeDescriptor* type = player_.project()->graph.registry().find(typeId);
    if (plan.has_value() && target.has_value() && type != nullptr) {
        std::vector<graph::NodeRect> rects;
        for (const auto& [id, item] : nodes_) {
            const QSizeF size = item->size();
            rects.push_back({item->nodeId(), static_cast<float>(item->pos().x()), static_cast<float>(item->pos().y()),
                             static_cast<float>(size.width()), static_cast<float>(size.height())});
        }
        const QSizeF size = NodeItem::sizeFor(*type);
        const QPointF corner = scenePos - QPointF{70.0, 20.0};
        const graph::Placement placement = graph::placeDockedNode(
            player_.project()->graph, rects, *plan, *target, Vec2{static_cast<float>(size.width()), static_cast<float>(size.height())},
            Vec2{static_cast<float>(corner.x()), static_cast<float>(corner.y())});
        if (addNodeAt(typeId, QPointF{static_cast<double>(placement.position.x), static_cast<double>(placement.position.y)}, &*plan,
                      &placement.moved)
                .has_value()) {
            ensureNodesVisible();
            return true;
        }
    }
    addNodeAt(typeId, scenePos - QPointF{70.0, 20.0});
    ensureNodesVisible();
    return true;
}

void NodeEditor::ensureNodesVisible() {
    if (nodes_.empty() || viewport()->width() < 40 || viewport()->height() < 40) {
        return;
    }
    const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    bool anyVisible = false;
    for (const auto& [id, item] : nodes_) {
        anyVisible = anyVisible || visible.intersects(item->sceneBoundingRect());
    }
    const double scale = transform().m11();
    if (!anyVisible || !std::isfinite(scale) || scale < 0.02) {
        showAll();
    }
}

bool NodeEditor::viewportEvent(QEvent* event) {
    switch (event->type()) {
        case QEvent::DragEnter:
            if (carriesNodeType(static_cast<QDragEnterEvent*>(event)->mimeData())) {
                dragEnterEvent(static_cast<QDragEnterEvent*>(event));
                return true;
            }
            break;
        case QEvent::DragMove:
            if (carriesNodeType(static_cast<QDragMoveEvent*>(event)->mimeData())) {
                dragMoveEvent(static_cast<QDragMoveEvent*>(event));
                return true;
            }
            break;
        case QEvent::DragLeave:
            clearDockPreview();
            break;
        case QEvent::Drop:
            if (carriesNodeType(static_cast<QDropEvent*>(event)->mimeData())) {
                dropEvent(static_cast<QDropEvent*>(event));
                return true;
            }
            break;
        default:
            break;
    }
    return QGraphicsView::viewportEvent(event);
}

std::optional<NodeId> NodeEditor::addNodeAtCenter(const std::string& typeId) {
    const QPointF center = mapToScene(viewport()->rect().center());
    return addNodeAt(typeId, center - QPointF{70.0, 20.0});
}

void NodeEditor::setDeleteZone(QWidget* zone) { deleteZone_ = zone; }

bool NodeEditor::overDeleteZone(const QPoint& globalPos) const {
    return deleteZone_ != nullptr && deleteZone_->isVisible() &&
           deleteZone_->rect().contains(deleteZone_->mapFromGlobal(globalPos));
}

}  // namespace whitehole::studio
