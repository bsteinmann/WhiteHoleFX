// node_editor_exercise.cpp: Rauchtest des Node-Editors für die CI (--exercise).
//
// Legt zwei Nodes an, zieht ein Kabel zwischen ihnen auf dem Weg der Maus, prüft die Suche, kopiert,
// fügt ein und dupliziert, löscht Kabel und Nodes wieder. Getrennt von node_editor.cpp, weil es mit
// dem normalen Betrieb nichts zu tun hat.
#include <QPointF>
#include <QString>
#include <QTransform>
#include <QTemporaryDir>

#include <cmath>
#include <filesystem>

#include <QByteArray>
#include <QMimeData>

#include "node_editor.hpp"
#include "node_info.hpp"
#include "node_palette.hpp"
#include "node_search.hpp"

namespace whitehole::studio {

QString NodeEditor::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("kein Projekt");
    }
    // Zoom: Auch eine Ansicht außerhalb von 0,25..3 (Fit bei vielen Nodes) muss sich wieder in den Bereich zoomen lassen.
    {
        const QTransform saved = transform();
        resetTransform();
        scale(0.1, 0.1);
        zoomBy(2.0);
        const double zoomedIn = transform().m11();
        zoomBy(0.5);
        const double notFurtherOut = transform().m11();
        resetTransform();
        scale(5.0, 5.0);
        zoomBy(0.5);
        const double zoomedOut = transform().m11();
        zoomBy(2.0);
        const double notFurtherIn = transform().m11();
        resetTransform();
        zoomBy(1.2);
        const double inRange = transform().m11();
        setTransform(saved);
        if (std::abs(zoomedIn - 0.2) > 1e-6 || std::abs(notFurtherOut - 0.2) > 1e-6 || std::abs(zoomedOut - 2.5) > 1e-6 ||
            std::abs(notFurtherIn - 3.0) > 1e-6 || std::abs(inRange - 1.2) > 1e-6) {
            return QStringLiteral("Zoom: %1 / %2 / %3 / %4 / %5, erwartet 0.2 / 0.2 / 2.5 / 3 / 1.2")
                .arg(zoomedIn).arg(notFurtherOut).arg(zoomedOut).arg(notFurtherIn).arg(inRange);
        }
    }

    // Jeder Node-Typ hat eine Beschreibung (Info-Zeichen), und sie erscheint im Node.
    if (const auto problems = nodeInfoProblems(player_.project()->graph.registry()); !problems.empty()) {
        return QStringLiteral("Node-Beschreibungen: %1 (und %2 weitere)").arg(QString::fromStdString(problems.front())).arg(problems.size() - 1);
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
    // Issue #29: Ein neuer Node ist der einzige ausgewählte, auch wenn vorher andere gewählt waren.
    if (scene_.selectedItems().size() != 1 || !itemOf(*sink)->isSelected()) {
        return QStringLiteral("Neuer Node: %1 Nodes ausgewählt, erwartet nur der neue").arg(scene_.selectedItems().size());
    }
    NodeItem* from = itemOf(*source);
    NodeItem* to = itemOf(*sink);
    if (from == nullptr || from->info().isEmpty() || !from->infoButtonAt(from->mapToScene(QPointF{152.0, 13.0}))) {
        return QStringLiteral("Info-Zeichen fehlt am neuen Node");
    }
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

    // Rechtsklick auf einen Port: passende Nodes anbieten, anlegen, verbinden.
    if (const QString problem = exerciseConnectMenu(); !problem.isEmpty()) {
        return problem;
    }

    // Einrasten beim Ziehen aus der Palette: auf ein Kabel (dazwischen), auf einen Node (links davor, rechts dahinter).
    if (const QString problem = exerciseDocking(*source, *sink); !problem.isEmpty()) {
        return problem;
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

    // Überbrücken: Schalter im Kopf trifft, Taste B und Menü benutzen dieselbe Funktion; ein Schritt Rückgängig.
    {
        NodeItem* shifted = itemOf(*sink);
        const QPointF buttonSpot = shifted->mapToScene(QPointF{166.0 + 8.0, 13.0});
        if (shifted == nullptr || !shifted->canBypass() || !shifted->bypassButtonAt(buttonSpot) ||
            shifted->bypassButtonAt(shifted->anchorScene(shifted->ports().front()))) {
            return QStringLiteral("Überbrücken: der Schalter im Kopf wird nicht getroffen");
        }
        if (!toggleBypass(*sink) || !itemOf(*sink)->bypassed() ||
            !player_.project()->graph.findNode(*sink)->bypassed) {
            return QStringLiteral("Überbrücken: Schalter ohne Wirkung");
        }
        scene_.clearSelection();
        itemOf(*sink)->setSelected(true);
        if (!toggleBypassOfSelection() || itemOf(*sink)->bypassed() || player_.project()->graph.findNode(*sink)->bypassed) {
            return QStringLiteral("Überbrücken: Taste B schaltet nicht wieder ein");
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

namespace whitehole::studio {

QString NodeEditor::exerciseDocking(NodeId source, NodeId sink) {
    const std::size_t nodesBefore = player_.project()->graph.nodes().size();
    const std::size_t wiresBefore = player_.project()->graph.connections().size();
    const std::string kColormap = "wh.modifier.colormap";

    // Auf einen Node: links speist der neue den freien Eingang, rechts hängt er hinter dem Ausgang.
    NodeItem* sinkItem = itemOf(sink);
    if (sinkItem == nullptr) {
        return QStringLiteral("Einrasten: Ziel-Node fehlt");
    }
    const auto onLeft = dockPlanAt("wh.generator.plasma", sinkItem->mapToScene(QPointF{30.0, 40.0}), false);
    if (!onLeft.has_value() || onLeft->links.size() != 1 || !(onLeft->links[0].to == graph::PortRef{sink, "source"})) {
        return QStringLiteral("Einrasten: linke Hälfte eines Nodes mit freiem Eingang");
    }
    const auto onRight = dockPlanAt(kColormap, sinkItem->mapToScene(QPointF{160.0, 40.0}), false);
    if (!onRight.has_value() || onRight->links.size() != 1 || !(onRight->links[0].from.node == sink)) {
        return QStringLiteral("Einrasten: rechte Hälfte eines Nodes");
    }
    if (dockPlanAt("wh.generator.plasma", sinkItem->mapToScene(QPointF{30.0, 40.0}), true).has_value()) {
        return QStringLiteral("Einrasten: mit Alt darf nichts einrasten");
    }

    // Auf ein Kabel: dazwischen, alles in einem Schritt für Rückgängig.
    if (!connectPorts(graph::PortRef{source, "out"}, graph::PortRef{sink, "source"})) {
        return QStringLiteral("Einrasten: Kabel für den Test anlegen");
    }
    const WireItem* wire = nullptr;
    for (const WireItem* candidate : wires_) {
        const graph::Connection* c = player_.project()->graph.findConnection(candidate->connectionId());
        if (c != nullptr && c->from.node == source && c->to.node == sink) {
            wire = candidate;
        }
    }
    if (wire == nullptr) {
        return QStringLiteral("Einrasten: kein Kabel zum Treffen");
    }
    const QPointF middle = wire->path().pointAtPercent(0.5);
    if (!dockPlanAt(kColormap, middle, false).has_value()) {
        return QStringLiteral("Einrasten: Kabel bei %1/%2 nicht getroffen").arg(middle.x()).arg(middle.y());
    }
    if (dockPlanAt(kColormap, middle, true).has_value() || dockPlanAt(kColormap, middle + QPointF{0.0, 4000.0}, false).has_value() ||
        dockPlanAt("wh.output.canvas", middle, false).has_value()) {
        return QStringLiteral("Einrasten: mit Alt, weit weg vom Kabel oder beim Ausgangs-Node darf nichts einrasten");
    }
    showDockPreview(kColormap, middle, false);
    if (dockWire_ == nullptr || dockLines_.size() != 2) {
        return QStringLiteral("Einrasten: Vorschau zeigt %1 gestrichelte Kabel, erwartet 2").arg(dockLines_.size());
    }
    clearDockPreview();
    if (dockWire_ != nullptr || !dockLines_.empty()) {
        return QStringLiteral("Einrasten: Vorschau bleibt stehen");
    }

    QMimeData mime;
    mime.setData(QString::fromLatin1(kNodeTypeMime), QByteArray{kColormap.c_str()});
    if (!handleNodeDrop(&mime, mapFromScene(middle))) {
        return QStringLiteral("Einrasten: Drop abgelehnt");
    }
    if (player_.project()->graph.nodes().size() != nodesBefore + 1 || player_.project()->graph.connections().size() != wiresBefore + 2 ||
        scene_.selectedItems().size() != 1) {
        return QStringLiteral("Einrasten auf ein Kabel: %1 Nodes, %2 Kabel, %3 ausgewählt; erwartet %4, %5, 1")
            .arg(player_.project()->graph.nodes().size())
            .arg(player_.project()->graph.connections().size())
            .arg(scene_.selectedItems().size())
            .arg(nodesBefore + 1)
            .arg(wiresBefore + 2);
    }
    // Der eingefügte Node überdeckt keinen anderen
    for (const auto& [idA, itemA] : nodes_) {
        for (const auto& [idB, itemB] : nodes_) {
            if (idA < idB && QRectF{itemA->pos(), itemA->size()}.intersects(QRectF{itemB->pos(), itemB->size()})) {
                return QStringLiteral("Einrasten: Node %1 und Node %2 überdecken sich nach dem Einfügen").arg(idA).arg(idB);
            }
        }
    }
    if (!player_.undo().ok()) {
        return QStringLiteral("Einrasten: Rückgängig scheitert");
    }
    rebuild();
    if (player_.project()->graph.nodes().size() != nodesBefore || player_.project()->graph.connections().size() != wiresBefore + 1) {
        return QStringLiteral("Einrasten: ein Rückgängig muss Node und Kabel zugleich zurücknehmen (%1 Nodes, %2 Kabel)")
            .arg(player_.project()->graph.nodes().size())
            .arg(player_.project()->graph.connections().size());
    }
    // Zustand wie vorher: das Test-Kabel wieder weg
    scene_.clearSelection();
    for (WireItem* candidate : wires_) {
        const graph::Connection* c = player_.project()->graph.findConnection(candidate->connectionId());
        if (c != nullptr && c->from.node == source && c->to.node == sink) {
            candidate->setSelected(true);  // nur das Test-Kabel, die übrigen Kabel des Projekts bleiben
        }
    }
    removeSelected();
    if (player_.project()->graph.connections().size() != wiresBefore) {
        return QStringLiteral("Einrasten: Test-Kabel nicht entfernt (%1 Kabel, erwartet %2)")
            .arg(player_.project()->graph.connections().size())
            .arg(wiresBefore);
    }
    return {};
}

}  // namespace whitehole::studio
