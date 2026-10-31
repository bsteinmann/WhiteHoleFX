// node_editor_exercise.cpp: Rauchtest des Node-Editors für die CI (--exercise).
//
// Legt zwei Nodes an, zieht ein Kabel zwischen ihnen auf dem Weg der Maus, prüft die Suche, kopiert,
// fügt ein und dupliziert, löscht Kabel und Nodes wieder. Getrennt von node_editor.cpp, weil es mit
// dem normalen Betrieb nichts zu tun hat.
#include <QPointF>
#include <QString>
#include <QTemporaryDir>

#include <filesystem>

#include "node_editor.hpp"
#include "node_search.hpp"

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

    // Suche: Groß-/Kleinschreibung egal, mehrere Wörter müssen alle passen.
    const auto all = player_.project()->graph.registry().all();
    const auto rain = filterNodeTypes(all, QStringLiteral("RAIN gener"));
    if (rain.empty() || rain.front()->typeId != "wh.generator.rain") {
        return QStringLiteral("Suche: \"RAIN gener\" findet den Digital-Rain-Node nicht");
    }
    if (!filterNodeTypes(all, QStringLiteral("gibtesnicht")).empty() ||
        filterNodeTypes(all, QString{}).size() != all.size()) {
        return QStringLiteral("Suche: leere Eingabe soll alles, Unsinn nichts finden");
    }

    // Kopieren, Einfügen und Duplizieren: jeweils ein Node mehr, danach wieder weg.
    const std::size_t nodesNow = nodes_.size();
    scene_.clearSelection();
    if (NodeItem* original = itemOf(*source)) {
        original->setSelected(true);
    }
    if (!copySelection() || !canPaste()) {
        return QStringLiteral("Kopieren: nichts in der Zwischenablage");
    }
    if (!paste(QPointF{300.0, 300.0}) || nodes_.size() != nodesNow + 1) {
        return QStringLiteral("Einfügen: %1 Nodes in der Szene, erwartet %2").arg(nodes_.size()).arg(nodesNow + 1);
    }
    removeSelected();  // die Kopie ist nach dem Einfügen ausgewählt
    if (nodes_.size() != nodesNow) {
        return QStringLiteral("Einfügen: die Kopie war nicht ausgewählt (%1 Nodes statt %2)").arg(nodes_.size()).arg(nodesNow);
    }
    scene_.clearSelection();
    itemOf(*source)->setSelected(true);
    if (!duplicateSelection() || nodes_.size() != nodesNow + 1) {
        return QStringLiteral("Duplizieren: %1 Nodes in der Szene, erwartet %2").arg(nodes_.size()).arg(nodesNow + 1);
    }
    removeSelected();
    if (nodes_.size() != nodesNow) {
        return QStringLiteral("Duplizieren: die Kopie war nicht ausgewählt");
    }

    // Presets: speichern, einfügen, löschen, in einem Wegwerf-Ordner statt im Ordner des Nutzers.
    {
        QTemporaryDir temp;
        if (!temp.isValid()) {
            return QStringLiteral("Preset: kein temporärer Ordner");
        }
        const QByteArray folder = temp.path().toUtf8();
        const runtime::PresetStore store{
            std::filesystem::path{std::u8string{reinterpret_cast<const char8_t*>(folder.constData()),
                                                static_cast<std::size_t>(folder.size())}}};
        const runtime::PresetStore* const userStore = presets_;
        presets_ = &store;
        QString problem;
        scene_.clearSelection();
        itemOf(*source)->setSelected(true);
        itemOf(*sink)->setSelected(true);
        const std::size_t before = nodes_.size();
        if (!savePreset(QStringLiteral("Rauchtest")) || store.list().size() != 1) {
            problem = QStringLiteral("Preset: Speichern");
        } else if (!insertPreset(QStringLiteral("Rauchtest"), QPointF{500.0, 500.0}) || nodes_.size() != before + 2) {
            problem = QStringLiteral("Preset: Einfügen (%1 Nodes statt %2)").arg(nodes_.size()).arg(before + 2);
        } else {
            removeSelected();  // die eingefügten Nodes sind ausgewählt
            if (nodes_.size() != before) {
                problem = QStringLiteral("Preset: die eingefügten Nodes waren nicht ausgewählt");
            } else if (!deletePreset(QStringLiteral("Rauchtest")) || !store.list().empty()) {
                problem = QStringLiteral("Preset: Löschen");
            }
        }
        presets_ = userStore;
        if (!problem.isEmpty()) {
            return problem;
        }
    }

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
