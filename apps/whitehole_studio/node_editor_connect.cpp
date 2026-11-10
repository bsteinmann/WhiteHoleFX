// node_editor_connect.cpp: Rechtsklick auf einen Ein- oder Ausgang bietet passende neue Nodes an (Teil von NodeEditor).
//
// Warum es das gibt: Wer ein Kabel ziehen will, muss sonst erst den Node suchen, ablegen und dann verbinden. Hier
// wählt man ihn gleich aus einer Liste, die nur Nodes mit einem passenden Port enthält; er wird neben den angeklickten
// Node gesetzt und verbunden. Die Regeln für "passt" sind dieselben wie beim Kabelziehen (graph::isConnectable).
#include <QAction>
#include <QMenu>

#include <algorithm>
#include <map>
#include <tuple>

#include "node_editor.hpp"
#include "whitehole/graph/port_family.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

/// Abstand zwischen dem angeklickten Node und dem neuen.
constexpr double kGap = 60.0;

}  // namespace

bool NodeEditor::connectNewNode(const graph::PortRef& port, bool portIsOutput, const std::string& typeId,
                                const std::string& newPortKey) {
    NodeItem* anchor = itemOf(port.node);
    const graph::NodeDescriptor* type = player_.project() != nullptr ? player_.project()->graph.registry().find(typeId) : nullptr;
    if (anchor == nullptr || type == nullptr) {
        return false;
    }
    // Höhe des angeklickten Ports übernehmen, damit das Kabel ungefähr waagerecht läuft.
    double portY = 0.0;
    for (const PortSlot& slot : anchor->ports()) {
        if (anchor->portRef(slot) == port && slot.output == portIsOutput) {
            portY = slot.anchor.y();
        }
    }
    const QSizeF newSize = NodeItem::sizeFor(*type);
    const double x = portIsOutput ? anchor->pos().x() + anchor->size().width() + kGap
                                  : anchor->pos().x() - newSize.width() - kGap;
    const QPointF where{x, anchor->pos().y() + portY - 20.0};
    const std::optional<NodeId> created = addNodeAt(typeId, where);
    if (!created.has_value()) {
        return false;
    }
    const graph::PortRef other{*created, newPortKey};
    return portIsOutput ? connectPorts(port, other) : connectPorts(other, port);
}

void NodeEditor::buildConnectMenu(QMenu& menu, const Hit& hit) {
    const project::Project* project = player_.project();
    if (project == nullptr || hit.node == nullptr || hit.port == nullptr) {
        return;
    }
    const graph::PortRef port = hit.node->portRef(*hit.port);
    const bool isOutput = hit.port->output;
    // Ein Eingang nimmt nur ein Kabel; ist er belegt, gibt es nichts anzubieten.
    if (!isOutput && project->graph.findInputConnection(port.node, port.portKey) != nullptr) {
        return;
    }
    std::vector<const graph::NodeDescriptor*> types = project->graph.registry().all();
    std::ranges::sort(types, [](const graph::NodeDescriptor* a, const graph::NodeDescriptor* b) {
        return std::tie(a->category, a->displayName) < std::tie(b->category, b->displayName);
    });
    QMenu* connectMenu = nullptr;
    std::map<std::string, QMenu*> categories;
    for (const graph::NodeDescriptor* type : types) {
        // Passende Ports des Typs; mehrere ergeben mehrere Einträge "Typ (Port)".
        std::vector<const graph::PortDescriptor*> fitting;
        for (const graph::PortDescriptor& candidate : type->ports) {
            const bool fits = isOutput ? candidate.direction == graph::PortDirection::Input &&
                                             graph::isConnectable(hit.port->type, candidate.type)
                                       : candidate.direction == graph::PortDirection::Output &&
                                             graph::isConnectable(candidate.type, hit.port->type);
            if (fits) {
                fitting.push_back(&candidate);
            }
        }
        if (fitting.empty()) {
            continue;
        }
        if (connectMenu == nullptr) {
            connectMenu = menu.addMenu(isOutput ? tr("Ausgang verbinden mit neuem Node") : tr("Eingang verbinden mit neuem Node"));
        }
        QMenu*& submenu = categories[type->category];
        if (submenu == nullptr) {
            submenu = connectMenu->addMenu(fromStd(type->category.empty() ? std::string{"Sonstige"} : type->category));
        }
        for (const graph::PortDescriptor* candidate : fitting) {
            const QString text = fitting.size() == 1 ? fromStd(type->displayName)
                                                     : QStringLiteral("%1 (%2)").arg(fromStd(type->displayName), fromStd(candidate->displayName));
            QAction* action = submenu->addAction(text);
            const std::string typeId = type->typeId;
            const std::string key = candidate->key;
            QObject::connect(action, &QAction::triggered, this,
                             [this, port, isOutput, typeId, key]() { connectNewNode(port, isOutput, typeId, key); });
        }
    }
}

namespace {

/// Das erste Blatt eines Menüs mit Untermenüs (die erste Aktion ohne eigenes Menü).
QAction* firstLeaf(const QMenu& menu) {
    for (QAction* action : menu.actions()) {
        if (action->menu() != nullptr) {
            if (QAction* leaf = firstLeaf(*action->menu())) {
                return leaf;
            }
        } else if (!action->isSeparator()) {
            return action;
        }
    }
    return nullptr;
}

}  // namespace

QString NodeEditor::exerciseConnectMenu() {
    const auto tryDirection = [this](bool output) -> QString {
        const QString what = output ? QStringLiteral("Ausgang") : QStringLiteral("Eingang");
        // Einen Port der gewünschten Richtung suchen, der etwas anzubieten hat (Eingänge: noch frei).
        for (const auto& entry : nodes_) {
            NodeItem* item = entry.second;
            for (const PortSlot& slot : item->ports()) {
                const graph::PortRef ref = item->portRef(slot);
                if (slot.output != output ||
                    (!output && player_.project()->graph.findInputConnection(ref.node, ref.portKey) != nullptr)) {
                    continue;
                }
                QMenu menu;
                buildConnectMenu(menu, Hit{item, &slot});
                QAction* leaf = firstLeaf(menu);
                if (leaf == nullptr) {
                    continue;
                }
                const std::size_t nodesBefore = nodes_.size();
                const std::size_t wiresBefore = player_.project()->graph.connections().size();
                leaf->trigger();
                if (nodes_.size() != nodesBefore + 1 || player_.project()->graph.connections().size() != wiresBefore + 1) {
                    return QStringLiteral("Port-Menü (%1): erwartet einen neuen Node und ein neues Kabel, es sind %2 Nodes und %3 Kabel")
                        .arg(what)
                        .arg(nodes_.size())
                        .arg(player_.project()->graph.connections().size());
                }
                removeSelected();  // der neue Node ist ausgewählt: samt Kabel wieder weg
                return nodes_.size() == nodesBefore ? QString{} : QStringLiteral("Port-Menü: neuer Node ließ sich nicht wieder entfernen");
            }
        }
        return QStringLiteral("Port-Menü: kein %1 mit passenden Nodes gefunden").arg(what);
    };
    for (const bool output : {true, false}) {
        if (const QString problem = tryDirection(output); !problem.isEmpty()) {
            return problem;
        }
    }
    return {};
}

}  // namespace whitehole::studio
