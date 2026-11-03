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
}

bool NodeEditor::handleNodeDrop(const QMimeData* mime, const QPoint& viewportPos) {
    if (!carriesNodeType(mime)) {
        return false;
    }
    const std::string typeId = nodeTypeOf(mime);
    const QPointF scenePos = mapToScene(viewportPos);
    const std::optional<graph::DockPlan> plan = dockPlanAt(typeId, scenePos, altPressed());
    clearDockPreview();
    // Der Mauszeiger steht in der Mitte des neuen Nodes
    if (plan.has_value() && addNodeAt(typeId, scenePos - QPointF{70.0, 20.0}, &*plan).has_value()) {
        return true;
    }
    addNodeAt(typeId, scenePos - QPointF{70.0, 20.0});
    return true;
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
