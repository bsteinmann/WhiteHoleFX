// node_editor_exercise.cpp: Rauchtest des Node-Editors für die CI (--exercise).
//
// Legt zwei Nodes an, zieht ein Kabel zwischen ihnen auf dem Weg der Maus, löscht Kabel und Nodes
// wieder. Getrennt von node_editor.cpp, weil es mit dem normalen Betrieb nichts zu tun hat.
#include <QPointF>
#include <QString>

#include "node_editor.hpp"

namespace whitehole::studio {

QString NodeEditor::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("kein Projekt");
    }
    const std::size_t nodesBefore = player_.project()->graph.nodes().size();
    const std::size_t wiresBefore = player_.project()->graph.connections().size();

    const auto freeSpot = [this]() {
        const QRectF content = scene_.itemsBoundingRect();
        return QPointF{content.left() + 20.0, content.bottom() + 80.0};
    };
    const std::optional<NodeId> source = addNodeAt("wh.generator.plasma", freeSpot());
    if (!source) {
        return QStringLiteral("Plasma-Node anlegen");
    }
    const std::optional<NodeId> sink = addNodeAt("wh.transform.mirror", freeSpot());
    if (!sink) {
        return QStringLiteral("Mirror-Node anlegen");
    }
    NodeItem* from = itemOf(*source);
    NodeItem* to = itemOf(*sink);
    if (from == nullptr || to == nullptr || from->findPort("out") == nullptr || to->findPort("source") == nullptr) {
        return QStringLiteral("Ports der neuen Nodes nicht gefunden");
    }

    // Wie mit der Maus: am Ausgang anfassen, über den Eingang ziehen, loslassen.
    const QPointF start = from->anchorScene(*from->findPort("out"));
    const QPointF end = to->anchorScene(*to->findPort("source"));
    if (!beginWire(start)) {
        return QStringLiteral("Ausgang bei %1/%2 nicht getroffen").arg(start.x()).arg(start.y());
    }
    moveWire((start + end) / 2.0);
    moveWire(end);
    const Hit probe = portAt(end);
    const QString probeNote = QStringLiteral("Treffer am Eingang: Node %1, Port %2, erwartet Node %3, Items an der Stelle %4")
                                  .arg(probe.node != nullptr ? static_cast<qulonglong>(probe.node->nodeId().value()) : 0ULL)
                                  .arg(probe.port != nullptr ? QString::fromStdString(probe.port->key) : QStringLiteral("-"))
                                  .arg(static_cast<qulonglong>(sink->value()))
                                  .arg(scene_.items(end).size());
    const bool connected = endWire(end);
    if (!connected || player_.project()->graph.connections().size() != wiresBefore + 1 ||
        wires_.size() != wiresBefore + 1) {
        return QStringLiteral("Kabel ziehen: endWire=%1, Verbindungen %2, Kabel %3, erwartet %4 (Eingang bei %5/%6); %7")
            .arg(connected)
            .arg(player_.project()->graph.connections().size())
            .arg(wires_.size())
            .arg(wiresBefore + 1)
            .arg(end.x())
            .arg(end.y())
            .arg(probeNote);
    }

    // Kabel auswählen und löschen. Die Auswahl überlebt Änderungen am Graphen; die Nodes
    // sollen hier nicht mehr ausgewählt sein, sonst würden sie mit gelöscht.
    scene_.clearSelection();
    for (WireItem* wire : wires_) {
        const auto* c = player_.project()->graph.findConnection(wire->connectionId());
        if (c != nullptr && c->from.node == *source) {
            wire->setSelected(true);
        }
    }
    removeSelected();
    if (player_.project()->graph.connections().size() != wiresBefore) {
        return QStringLiteral("Kabel löschen: %1 Verbindungen, erwartet %2").arg(player_.project()->graph.connections().size()).arg(wiresBefore);
    }

    // Verschieben wie mit der Maus: Position ändern, Änderung übernehmen
    from = itemOf(*source);
    if (from == nullptr) {
        return QStringLiteral("Kabel löschen: Der Quell-Node ist nach dem Löschen des Kabels weg");
    }
    from->setPos(from->pos() + QPointF{30.0, 10.0});
    commitMoves();

    // Beide Nodes auswählen und löschen
    NodeItem* sourceItem = itemOf(*source);
    NodeItem* sinkItem = itemOf(*sink);
    if (sourceItem == nullptr || sinkItem == nullptr) {
        return QStringLiteral("Aufräumen: Nodes fehlen in der Szene");
    }
    sourceItem->setSelected(true);
    sinkItem->setSelected(true);
    removeSelected();
    if (player_.project()->graph.nodes().size() != nodesBefore || nodes_.size() != nodesBefore) {
        return QStringLiteral("Aufräumen: %1 Nodes im Graph, %2 in der Szene, erwartet %3")
            .arg(player_.project()->graph.nodes().size())
            .arg(nodes_.size())
            .arg(nodesBefore);
    }
    return {};
}

}  // namespace whitehole::studio
