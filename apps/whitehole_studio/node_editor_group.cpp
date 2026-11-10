// node_editor_group.cpp: Gruppen im Node-Editor (Kachel, Rahmen, Dialog, Menüs).
//
// Eine Gruppe (graph/node_group.hpp) ist nur Beschriftung über echten Nodes. Zugeklappt ersetzt eine Kachel die
// Mitglieder: ihre Ports stehen für Ports der Mitglieder (PortSlot::realNode/realKey), die Kabel laufen im Graphen
// unverändert und werden im Bild an die Kachel gezogen. Aufgeklappt sind die Mitglieder normale Nodes in einem Rahmen.
// Alle Änderungen gehen über den Player (Rückgängig), danach wird die Szene neu gebaut.
#include <QAction>
#include <QColor>
#include <QMenu>
#include <QMouseEvent>

#include <algorithm>
#include <set>

#include "group_dialog.hpp"
#include "node_editor.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }
std::string toStd(const QString& text) { return text.toUtf8().toStdString(); }

QColor toQColor(const Vec3& c) {
    const auto channel = [](float v) { return std::clamp(static_cast<double>(v), 0.0, 1.0); };
    return QColor::fromRgbF(channel(c.x), channel(c.y), channel(c.z));
}

Vec3 fromQColor(const QColor& c) {
    return Vec3{static_cast<float>(c.redF()), static_cast<float>(c.greenF()), static_cast<float>(c.blueF())};
}

bool contains(const std::vector<NodeId>& list, NodeId id) { return std::ranges::find(list, id) != list.end(); }

/// Titel eines Nodes wie im Editor ("Plasma 2" bei mehreren vom selben Typ), auch wenn er verdeckt ist.
QString titleOf(const graph::Graph& g, NodeId id) {
    const graph::NodeInstance* node = g.findNode(id);
    const graph::NodeDescriptor* descriptor = node != nullptr ? g.registry().find(node->typeId) : nullptr;
    if (descriptor == nullptr) {
        return {};
    }
    int total = 0;
    int index = 0;
    for (const graph::NodeInstance& other : g.nodes()) {
        if (other.typeId == node->typeId) {
            ++total;
            if (other.id == id) {
                index = total;
            }
        }
    }
    QString title = fromStd(descriptor->displayName);
    if (total > 1) {
        title += QStringLiteral(" %1").arg(index);
    }
    return title;
}

}  // namespace

// --- Bild --------------------------------------------------------------------------------------

NodeEditor::Endpoint NodeEditor::endpointOf(const graph::PortRef& ref) const {
    if (const auto tile = tiles_.find(ref.node.value()); tile != tiles_.end()) {
        for (const PortSlot& slot : tile->second->ports()) {
            if (slot.realNode == ref.node.value() && slot.realKey == ref.portKey) {
                return {tile->second, slot.key};
            }
        }
        return {};  // ein Port im Inneren der zugeklappten Gruppe: kein Kabel im Bild
    }
    if (const auto item = nodes_.find(ref.node.value()); item != nodes_.end()) {
        return {item->second, ref.portKey};
    }
    return {};
}

const graph::NodeGroup* NodeEditor::groupOfItem(const NodeItem& item) const {
    return player_.project() != nullptr ? player_.project()->graph.groupOf(item.nodeId()) : nullptr;
}

std::vector<NodeId> NodeEditor::groupMembersOf(const NodeItem& tile) const {
    const graph::NodeGroup* group = groupOfItem(tile);
    return group != nullptr ? group->members : std::vector<NodeId>{};
}

const graph::NodeGroup* NodeEditor::groupAt(const QPointF& scenePos) const {
    if (player_.project() == nullptr) {
        return nullptr;
    }
    for (QGraphicsItem* item : scene_.items(scenePos)) {
        if (const auto* node = dynamic_cast<const NodeItem*>(item)) {
            if (const graph::NodeGroup* group = groupOfItem(*node)) {
                return group;
            }
        } else if (const auto* frame = dynamic_cast<const GroupFrameItem*>(item)) {
            return player_.project()->graph.findGroup(toStd(frame->name()));
        }
    }
    return nullptr;
}

void NodeEditor::onItemMoved(NodeItem& moved) {
    for (WireItem* wire : wires_) {
        if (wire->fromNode() == &moved || wire->toNode() == &moved) {
            wire->updatePath();
        }
    }
    for (GroupFrameItem* frame : frames_) {
        frame->updateRect();
    }
}

bool NodeEditor::moveGroupTile(const NodeItem& tile, Vec2 position) {
    const graph::NodeGroup* group = groupOfItem(tile);
    return group != nullptr && player_.moveNodeGroup(group->name, position).ok();
}

void NodeEditor::buildGroupItems() {
    const graph::Graph& g = player_.project()->graph;
    for (const graph::NodeGroup& group : g.groups()) {
        const QColor color = toQColor(group.color);
        if (!group.collapsed) {
            std::vector<NodeItem*> items;
            for (const NodeId id : group.members) {
                if (const auto it = nodes_.find(id.value()); it != nodes_.end()) {
                    items.push_back(it->second);
                }
            }
            if (!items.empty()) {
                auto* frame = new GroupFrameItem{fromStd(group.name), color, std::move(items)};
                scene_.addItem(frame);
                frames_.push_back(frame);
            }
            continue;
        }

        // Ports der Kachel: die gewählten, dazu jeder Anschluss mit einem Kabel nach außen (sonst wäre es unsichtbar)
        std::vector<PortSlot> slots;
        std::set<std::pair<std::uint64_t, std::string>> have;
        const auto add = [&](const graph::PortRef& ref, const QString& label) {
            if (!have.emplace(ref.node.value(), ref.portKey).second) {
                return;
            }
            const graph::NodeInstance* node = g.findNode(ref.node);
            const graph::NodeDescriptor* descriptor = node != nullptr ? g.registry().find(node->typeId) : nullptr;
            const graph::PortDescriptor* port = descriptor != nullptr ? descriptor->findPort(ref.portKey) : nullptr;
            if (port == nullptr) {
                return;
            }
            PortSlot slot;
            slot.key = "g" + std::to_string(slots.size());
            slot.label = label.isEmpty() ? fromStd(port->displayName) : label;
            slot.type = port->type;
            slot.output = port->direction == graph::PortDirection::Output;
            slot.realNode = ref.node.value();
            slot.realKey = ref.portKey;
            slots.push_back(std::move(slot));
        };
        for (const graph::GroupPort& port : group.ports) {
            add(graph::PortRef{port.node, port.portKey}, fromStd(port.label));
        }
        for (const graph::Connection& c : g.connections()) {
            const bool fromInside = contains(group.members, c.from.node);
            const bool toInside = contains(group.members, c.to.node);
            if (fromInside != toInside) {
                add(fromInside ? c.from : c.to, QString{});
            }
        }
        auto* tile = new NodeItem{group.members.front(), fromStd(group.name), std::move(slots), color};
        tile->setPos(static_cast<qreal>(group.position.x), static_cast<qreal>(group.position.y));
        scene_.addItem(tile);
        tile->setMoveCallback([this](NodeItem& moved) { onItemMoved(moved); });
        nodes_.emplace(group.members.front().value(), tile);
        for (const NodeId id : group.members) {
            tiles_[id.value()] = tile;
        }
        ++tileCount_;
    }
}

// --- Bedienung ---------------------------------------------------------------------------------

bool NodeEditor::openGroupDialog(const graph::NodeGroup* existing, const std::vector<NodeId>& members) {
    const project::Project* project = player_.project();
    if (project == nullptr || members.empty()) {
        return false;
    }
    const graph::Graph& g = project->graph;

    std::vector<GroupDialogMember> rows;
    for (const NodeId id : members) {
        const graph::NodeInstance* node = g.findNode(id);
        const graph::NodeDescriptor* descriptor = node != nullptr ? g.registry().find(node->typeId) : nullptr;
        if (descriptor == nullptr) {
            continue;
        }
        GroupDialogMember row;
        row.id = id;
        row.title = titleOf(g, id);
        for (const graph::PortDescriptor& port : descriptor->ports) {
            GroupDialogEntry entry;
            entry.key = port.key;
            entry.name = fromStd(port.displayName);
            entry.label = entry.name;
            entry.output = port.direction == graph::PortDirection::Output;
            if (existing != nullptr) {
                for (const graph::GroupPort& chosen : existing->ports) {
                    if (chosen.node == id && chosen.portKey == port.key) {
                        entry.checked = true;
                        entry.label = fromStd(chosen.label);
                    }
                }
            } else {
                // Vorschlag: was nach außen verbunden ist, dazu freie Ausgänge (damit die Kachel etwas liefert)
                bool connectedOutside = false;
                bool connectedAtAll = false;
                for (const graph::Connection& c : g.connections()) {
                    const graph::PortRef& mine = entry.output ? c.from : c.to;
                    const graph::PortRef& other = entry.output ? c.to : c.from;
                    if (mine.node == id && mine.portKey == port.key) {
                        connectedAtAll = true;
                        connectedOutside = connectedOutside || !contains(members, other.node);
                    }
                }
                entry.checked = connectedOutside || (entry.output && !connectedAtAll);
            }
            row.ports.push_back(std::move(entry));
        }
        for (const graph::ParameterDescriptor& parameter : descriptor->parameters) {
            GroupDialogEntry entry;
            entry.key = parameter.key;
            entry.name = fromStd(parameter.displayName);
            entry.label = entry.name;
            if (existing != nullptr) {
                for (const graph::GroupParameter& chosen : existing->parameters) {
                    if (chosen.node == id && chosen.key == parameter.key) {
                        entry.checked = true;
                        entry.label = fromStd(chosen.label);
                    }
                }
            }
            row.parameters.push_back(std::move(entry));
        }
        rows.push_back(std::move(row));
    }

    std::vector<QString> taken;
    for (const graph::NodeGroup& other : g.groups()) {
        if (existing == nullptr || other.name != existing->name) {
            taken.push_back(fromStd(other.name));
        }
    }
    QString suggestion = existing != nullptr ? fromStd(existing->name) : QString{};
    for (int n = 1; suggestion.isEmpty(); ++n) {
        const QString candidate = tr("Gruppe %1").arg(n);
        if (std::ranges::find(taken, candidate) == taken.end()) {
            suggestion = candidate;
        }
    }
    GroupDialog dialog{existing != nullptr ? tr("Gruppe bearbeiten") : tr("Gruppieren"),
                       suggestion,
                       existing != nullptr ? toQColor(existing->color) : QColor{64, 140, 255},
                       std::move(rows),
                       std::move(taken),
                       this};
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    graph::NodeGroup result = existing != nullptr ? *existing : graph::NodeGroup{};
    result.name = toStd(dialog.name());
    result.color = fromQColor(dialog.color());
    result.members = members;
    result.ports.clear();
    result.parameters.clear();
    for (const GroupDialogChoice& choice : dialog.chosenPorts()) {
        result.ports.push_back(graph::GroupPort{choice.node, choice.entry.key, toStd(choice.entry.label)});
    }
    for (const GroupDialogChoice& choice : dialog.chosenParameters()) {
        result.parameters.push_back(graph::GroupParameter{choice.node, choice.entry.key, toStd(choice.entry.label)});
    }
    if (existing == nullptr) {
        result.collapsed = true;
        bool first = true;
        for (const NodeId id : members) {
            if (const graph::NodeInstance* node = g.findNode(id)) {
                result.position.x = first ? node->position.x : std::min(result.position.x, node->position.x);
                result.position.y = first ? node->position.y : std::min(result.position.y, node->position.y);
                first = false;
            }
        }
    }
    const auto r = existing != nullptr ? player_.replaceNodeGroup(existing->name, result) : player_.addNodeGroup(result);
    if (!r.ok()) {
        fail(r.error().message);
        return false;
    }
    scene_.clearSelection();
    rebuild();
    if (NodeItem* tile = itemOf(members.front()); tile != nullptr && result.collapsed) {
        tile->setSelected(true);
    }
    Q_EMIT edited(true);
    return true;
}

bool NodeEditor::groupSelection() {
    if (player_.project() == nullptr) {
        return false;
    }
    const std::vector<NodeId> ids = selectedNodes();
    if (ids.empty()) {
        fail("Es sind keine Nodes ausgewählt");
        return false;
    }
    for (const NodeId id : ids) {
        if (player_.project()->graph.groupOf(id) != nullptr) {
            fail("Ein ausgewählter Node steckt schon in einer Gruppe");
            return false;
        }
    }
    return openGroupDialog(nullptr, ids);
}

bool NodeEditor::editGroup(const std::string& name) {
    const graph::NodeGroup* group = player_.project() != nullptr ? player_.project()->graph.findGroup(name) : nullptr;
    if (group == nullptr) {
        return false;
    }
    const graph::NodeGroup copy = *group;  // der Dialog läuft, ohne dass der Graph sich unter ihm ändern darf
    return openGroupDialog(&copy, copy.members);
}

bool NodeEditor::ungroup(const std::string& name) {
    const graph::NodeGroup* group = player_.project() != nullptr ? player_.project()->graph.findGroup(name) : nullptr;
    if (group == nullptr) {
        return false;
    }
    const std::vector<NodeId> members = group->members;
    // Aus der Kachel werden wieder einzelne Nodes: erst aufklappen (Nodes an die Stelle der Kachel), dann auflösen.
    if (group->collapsed) {
        if (const auto r = player_.setNodeGroupCollapsed(name, false); !r.ok()) {
            fail(r.error().message);
            return false;
        }
    }
    if (const auto r = player_.removeNodeGroup(name); !r.ok()) {
        fail(r.error().message);
        return false;
    }
    scene_.clearSelection();
    rebuild();
    for (const NodeId id : members) {
        if (NodeItem* item = itemOf(id)) {
            item->setSelected(true);
        }
    }
    Q_EMIT edited(true);
    return true;
}

bool NodeEditor::setGroupCollapsed(const std::string& name, bool collapsed) {
    const graph::NodeGroup* group = player_.project() != nullptr ? player_.project()->graph.findGroup(name) : nullptr;
    if (group == nullptr) {
        return false;
    }
    const NodeId front = group->members.front();
    if (const auto r = player_.setNodeGroupCollapsed(name, collapsed); !r.ok()) {
        fail(r.error().message);
        return false;
    }
    scene_.clearSelection();
    rebuild();
    if (NodeItem* item = itemOf(front)) {
        item->setSelected(true);
    }
    Q_EMIT edited(true);
    return true;
}

void NodeEditor::addGroupMenu(QMenu& menu, NodeItem* node, const QPointF& scenePos) {
    const graph::NodeGroup* group = node != nullptr ? groupOfItem(*node) : groupAt(scenePos);
    if (group != nullptr) {
        const std::string name = group->name;
        menu.addSeparator();
        if (group->collapsed) {
            connect(menu.addAction(tr("Gruppe aufklappen")), &QAction::triggered, this, [this, name]() { setGroupCollapsed(name, false); });
        } else {
            connect(menu.addAction(tr("Gruppe zuklappen")), &QAction::triggered, this, [this, name]() { setGroupCollapsed(name, true); });
        }
        connect(menu.addAction(tr("Gruppe bearbeiten …")), &QAction::triggered, this, [this, name]() { editGroup(name); });
        connect(menu.addAction(tr("Gruppe auflösen")), &QAction::triggered, this, [this, name]() { ungroup(name); });
        return;
    }
    if (node != nullptr) {
        menu.addSeparator();
        connect(menu.addAction(tr("Gruppieren …\tStrg+G")), &QAction::triggered, this, [this]() { groupSelection(); });
    }
}

void NodeEditor::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        NodeItem* node = nodeAt(mapToScene(event->position().toPoint()));
        if (node != nullptr && node->isGroupTile() && portAt(mapToScene(event->position().toPoint())).node == nullptr) {
            if (const graph::NodeGroup* group = groupOfItem(*node)) {
                setGroupCollapsed(group->name, false);
                event->accept();
                return;
            }
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

}  // namespace whitehole::studio
