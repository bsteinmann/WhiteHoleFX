// node_editor_clipboard.cpp: Kopieren, Einfügen und Duplizieren im Node-Editor.
//
// Die eigentliche Arbeit (Nodes samt Kabeln kopieren, als ein Schritt einfügen) steckt in
// runtime/node_clipboard und Player::pasteNodes. Hier geht es nur um die Bedienung: wohin eingefügt
// wird und dass die Kopie danach ausgewählt ist.
#include <QCursor>

#include "node_editor.hpp"

namespace whitehole::studio {

std::vector<NodeId> NodeEditor::selectedNodes() const {
    std::vector<NodeId> ids;
    for (const auto& [id, item] : nodes_) {
        if (item->isSelected()) {
            ids.push_back(item->nodeId());
        }
    }
    return ids;
}

bool NodeEditor::copySelection() {
    const std::vector<NodeId> ids = selectedNodes();
    if (ids.empty() || player_.project() == nullptr) {
        return false;
    }
    clipboard_ = runtime::copyNodes(player_.project()->graph, ids);
    return !clipboard_.empty();
}

bool NodeEditor::pasteWithOffset(const runtime::NodeClipboard& clip, const QPointF& offset) {
    const auto pasted = player_.pasteNodes(clip, Vec2{static_cast<float>(offset.x()), static_cast<float>(offset.y())});
    if (!pasted.ok()) {
        fail(pasted.error().message);
        return false;
    }
    rebuild();
    scene_.clearSelection();
    for (const NodeId id : pasted.value()) {
        if (NodeItem* item = itemOf(id)) {
            item->setSelected(true);
        }
    }
    Q_EMIT edited(true);
    return true;
}

bool NodeEditor::paste(const std::optional<QPointF>& scenePos) {
    if (clipboard_.empty()) {
        return false;
    }
    const Vec2 corner = clipboard_.topLeft();
    QPointF target{static_cast<qreal>(corner.x) + 40.0, static_cast<qreal>(corner.y) + 40.0};
    if (scenePos) {
        target = *scenePos;
    } else {
        const QPoint local = viewport()->mapFromGlobal(QCursor::pos());
        if (viewport()->rect().contains(local)) {
            target = mapToScene(local);
        }
    }
    return pasteWithOffset(clipboard_, target - QPointF{static_cast<qreal>(corner.x), static_cast<qreal>(corner.y)});
}

bool NodeEditor::duplicateSelection() {
    const std::vector<NodeId> ids = selectedNodes();
    if (ids.empty() || player_.project() == nullptr) {
        return false;
    }
    // Die Zwischenablage bleibt unberührt; die Kopie sitzt leicht versetzt neben dem Original.
    return pasteWithOffset(runtime::copyNodes(player_.project()->graph, ids), QPointF{40.0, 40.0});
}

}  // namespace whitehole::studio
