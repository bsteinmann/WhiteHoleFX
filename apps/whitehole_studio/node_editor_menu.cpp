// node_editor_menu.cpp: Kontextmenü des Node-Editors.
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

#include <algorithm>
#include <map>
#include <tuple>

#include "node_editor.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

void NodeEditor::buildAddMenu(QMenu& menu, const QPointF& scenePos) {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    std::vector<const graph::NodeDescriptor*> types = project->graph.registry().all();
    std::ranges::sort(types, [](const graph::NodeDescriptor* a, const graph::NodeDescriptor* b) {
        return std::tie(a->category, a->displayName) < std::tie(b->category, b->displayName);
    });
    QMenu* add = menu.addMenu(tr("Node hinzufügen"));
    std::map<std::string, QMenu*> categories;
    for (const graph::NodeDescriptor* type : types) {
        QMenu*& submenu = categories[type->category];
        if (submenu == nullptr) {
            submenu = add->addMenu(fromStd(type->category.empty() ? std::string{"Sonstige"} : type->category));
        }
        QAction* action = submenu->addAction(fromStd(type->displayName));
        const std::string typeId = type->typeId;
        connect(action, &QAction::triggered, this, [this, typeId, scenePos]() { addNodeAt(typeId, scenePos); });
    }
}

void NodeEditor::contextMenuEvent(QContextMenuEvent* event) {
    if (player_.project() == nullptr) {
        return;
    }
    const QPointF scenePos = mapToScene(event->pos());
    QMenu menu{this};

    const Hit hit = portAt(scenePos);
    NodeItem* node = hit.node != nullptr ? hit.node : nodeAt(scenePos);
    WireItem* wire = wireAt(scenePos);

    if (hit.port != nullptr && hit.port->output && hit.port->type == graph::PortType::Field) {
        const graph::PortRef port{hit.node->nodeId(), hit.port->key};
        QAction* action = menu.addAction(tr("Als Ausgang setzen (output)"));
        connect(action, &QAction::triggered, this, [this, port]() { setTarget(port); });
    }
    if (node != nullptr) {
        if (!node->isSelected()) {
            scene()->clearSelection();
            node->setSelected(true);
        }
        connect(menu.addAction(tr("Kopieren\tStrg+C")), &QAction::triggered, this, [this]() { copySelection(); });
        connect(menu.addAction(tr("Duplizieren\tStrg+D")), &QAction::triggered, this,
                [this]() { duplicateSelection(); });
        menu.addSeparator();
        QAction* action = menu.addAction(tr("Node löschen"));
        connect(action, &QAction::triggered, this, [this, node]() {
            scene()->clearSelection();
            node->setSelected(true);
            removeSelected();
        });
    } else if (wire != nullptr) {
        QAction* action = menu.addAction(tr("Verbindung trennen"));
        connect(action, &QAction::triggered, this, [this, wire]() {
            scene()->clearSelection();
            wire->setSelected(true);
            removeSelected();
        });
    } else {
        connect(menu.addAction(tr("Node suchen …\tStrg+Leertaste")), &QAction::triggered, this,
                [this, scenePos]() { showSearch(scenePos); });
        buildAddMenu(menu, scenePos);
        QAction* pasteAction = menu.addAction(tr("Einfügen\tStrg+V"));
        pasteAction->setEnabled(canPaste());
        connect(pasteAction, &QAction::triggered, this, [this, scenePos]() { paste(scenePos); });
    }
    if (!menu.isEmpty()) {
        menu.exec(event->globalPos());
    }
    event->accept();
}

}  // namespace whitehole::studio
