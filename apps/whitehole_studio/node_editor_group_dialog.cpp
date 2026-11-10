// node_editor_group_dialog.cpp: Der Dialog "Gruppieren" / "Gruppe bearbeiten" im Node-Editor (Teil von NodeEditor).
//
// Warum eine eigene Datei: node_editor_group.cpp ist an der Größengrenze. Hier wird der Dialog mit den Mitgliedern gefüllt
// und aus der Antwort eine Gruppe gebaut (Vorschläge: verbundene Anschlüsse nach außen, freie Ausgänge).
#include <QColor>

#include <algorithm>

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
    if (existing != nullptr) {
        dialog.hideLibraryOption();
    }
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
    if (existing == nullptr && dialog.saveToLibrary() && groups_ != nullptr) {
        saveGroupToLibrary(result.name, true);
    }
    return true;
}

}  // namespace whitehole::studio
