// node_editor_drag.cpp: Drag & Drop zwischen Node-Palette und Editor (Teil von NodeEditor).
//
// Hinzufügen: Die Palette startet einen Qt-Drag mit dem Typ als MIME-Daten; der Editor legt beim
// Loslassen den Node an der Stelle an.
// Entfernen: Das Verschieben von Nodes ist kein Qt-Drag, sondern Bewegen von Szene-Items. Darum prüft
// der Editor beim Loslassen der Maus selbst, ob sie über der "Löschzone" (der Palette) steht.
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

#include "node_editor.hpp"
#include "node_palette.hpp"

namespace whitehole::studio {

namespace {

bool carriesNodeType(const QMimeData* mime) { return mime != nullptr && mime->hasFormat(QString::fromLatin1(kNodeTypeMime)); }

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
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void NodeEditor::dropEvent(QDropEvent* event) {
    if (!carriesNodeType(event->mimeData())) {
        QGraphicsView::dropEvent(event);
        return;
    }
    const std::string typeId = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(kNodeTypeMime))).toStdString();
    // Der Mauszeiger steht in der Mitte des neuen Nodes
    addNodeAt(typeId, mapToScene(event->position().toPoint()) - QPointF{70.0, 20.0});
    event->acceptProposedAction();
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
