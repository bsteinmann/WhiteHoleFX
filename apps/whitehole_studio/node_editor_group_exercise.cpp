// node_editor_group_exercise.cpp: Rauchtest der Gruppen im Node-Editor (Teil von NodeEditor::exercise).
//
// Warum es das gibt: Der Test läuft in der CI in einem unsichtbaren Fenster und prüft, was sich ohne Maus prüfen lässt:
// Kachel statt Mitglieder, Kabel an der Kachel, Verschieben, Aufklappen mit Rahmen, Auflösen, Löschen der Kachel.
#include <QByteArray>
#include <QColor>
#include <QTemporaryDir>

#include <cmath>
#include <filesystem>

#include "group_dialog.hpp"
#include "node_editor.hpp"

namespace whitehole::studio {

QString NodeEditor::exerciseGroups() {
    const auto a = addNodeAt("wh.generator.plasma", QPointF{-600.0, 600.0});
    const auto b = addNodeAt("wh.transform.mirror", QPointF{-350.0, 600.0});
    const auto c = addNodeAt("wh.transform.rotate", QPointF{-100.0, 600.0});
    if (!a || !b || !c) {
        return QStringLiteral("Gruppen: Test-Nodes nicht angelegt");
    }
    // Aufräumen, egal wie der Test endet: die Test-Nodes samt Gruppen wieder weg
    const auto cleanup = [&]() {
        (void)player_.removeNodeGroup("Rauchtest-Gruppe");
        for (const NodeId id : {*a, *b, *c}) {
            (void)player_.removeNode(id);
        }
        rebuild();
    };
    const auto giveUp = [&](const QString& what) {
        cleanup();
        return QStringLiteral("Gruppen: %1").arg(what);
    };
    if (!connectPorts({*a, "out"}, {*b, "source"}) || !connectPorts({*b, "out"}, {*c, "source"})) {
        return giveUp(QStringLiteral("Test-Kabel nicht angelegt"));
    }
    const std::size_t nodesBefore = nodes_.size();
    const std::size_t wiresBefore = wires_.size();

    graph::NodeGroup group;
    group.name = "Rauchtest-Gruppe";
    group.members = {*a, *b};
    group.ports = {graph::GroupPort{*b, "out", "Ausgang"}};
    group.position = Vec2{-600.0F, 600.0F};
    if (const auto added = player_.addNodeGroup(group); !added.ok()) {
        return giveUp(QString::fromStdString(added.error().message));
    }
    rebuild();
    if (groupTileCount() != 1 || nodes_.size() != nodesBefore - 1 || wires_.size() != wiresBefore - 1) {
        return giveUp(QStringLiteral("zugeklappt: %1 Kacheln, %2 Nodes (erwartet %3), %4 Kabel (erwartet %5)")
                        .arg(groupTileCount())
                        .arg(nodes_.size())
                        .arg(nodesBefore - 1)
                        .arg(wires_.size())
                        .arg(wiresBefore - 1));
    }
    NodeItem* tile = itemOf(*a);
    if (tile == nullptr || !tile->isGroupTile() || itemOf(*b) != tile) {
        return giveUp(QStringLiteral("die Kachel steht nicht für beide Mitglieder"));
    }
    const PortSlot* output = nullptr;
    for (const PortSlot& slot : tile->ports()) {
        output = slot.output ? &slot : output;
    }
    if (output == nullptr || !(tile->portRef(*output) == graph::PortRef{*b, "out"})) {
        return giveUp(QStringLiteral("der Ausgang der Kachel steht nicht für mirror.out"));
    }
    bool wiredToTile = false;
    for (const WireItem* wire : wires_) {
        wiredToTile = wiredToTile || wire->fromNode() == tile;
    }
    if (!wiredToTile) {
        return giveUp(QStringLiteral("das Kabel nach außen läuft nicht an der Kachel"));
    }

    // Kachel verschieben wie mit der Maus
    tile->setPos(tile->pos() + QPointF{40.0, 20.0});
    commitMoves();
    const graph::NodeGroup* stored = player_.project()->graph.findGroup(group.name);
    if (stored == nullptr || std::abs(stored->position.x - (-560.0F)) > 0.01F) {
        return giveUp(QStringLiteral("die verschobene Kachel wurde nicht übernommen"));
    }

    // Aufklappen: Nodes einzeln wieder da, Rahmen statt Kachel, Nodes an der Stelle der Kachel
    if (!setGroupCollapsed(group.name, false) || groupTileCount() != 0 || groupFrameCount() != 1 || nodes_.size() != nodesBefore ||
        wires_.size() != wiresBefore) {
        return giveUp(QStringLiteral("aufgeklappt: %1 Kacheln, %2 Rahmen, %3 Nodes, %4 Kabel")
                        .arg(groupTileCount())
                        .arg(groupFrameCount())
                        .arg(nodes_.size())
                        .arg(wires_.size()));
    }
    if (itemOf(*a) == nullptr || std::abs(itemOf(*a)->pos().x() - (-560.0)) > 0.5) {
        return giveUp(QStringLiteral("aufgeklappt: die Nodes sitzen nicht an der Stelle der Kachel"));
    }

    // Auflösen: keine Gruppe, kein Rahmen, alle Nodes bleiben
    if (!ungroup(group.name) || !player_.project()->graph.groups().empty() || groupFrameCount() != 0 || nodes_.size() != nodesBefore) {
        return giveUp(QStringLiteral("Auflösen"));
    }

    // Entf auf der Kachel löscht die Gruppe samt Nodes
    if (const auto added = player_.addNodeGroup(group); !added.ok()) {
        return giveUp(QString::fromStdString(added.error().message));
    }
    rebuild();

    // Bibliothek: speichern, einfügen (der Name wird frei gemacht), mit einem Rückgängig wieder weg
    {
        QTemporaryDir temp;
        if (!temp.isValid()) {
            return giveUp(QStringLiteral("kein temporärer Ordner"));
        }
        const QByteArray folder = temp.path().toUtf8();
        const runtime::PresetStore store{
            std::filesystem::path{std::u8string{reinterpret_cast<const char8_t*>(folder.constData()), static_cast<std::size_t>(folder.size())}}};
        const runtime::PresetStore* const userStore = groups_;
        groups_ = &store;
        const std::size_t groupsBefore = player_.project()->graph.groups().size();
        QString problem;
        if (!saveGroupToLibrary(group.name, false) || store.list().size() != 1) {
            problem = QStringLiteral("Speichern");
        } else if (!insertLibraryItem(QStringLiteral("group"), QString::fromStdString(group.name), QPointF{-300.0, 900.0}) ||
                   player_.project()->graph.groups().size() != groupsBefore + 1 ||
                   player_.project()->graph.findGroup(group.name + " 2") == nullptr) {
            problem = QStringLiteral("Einfügen");
        } else if (!player_.undo().ok()) {
            problem = QStringLiteral("Rückgängig");
        } else {
            rebuild();
            if (player_.project()->graph.groups().size() != groupsBefore) {
                problem = QStringLiteral("Rückgängig nimmt die Gruppe nicht zurück");
            }
        }
        groups_ = userStore;
        if (!problem.isEmpty()) {
            return giveUp(QStringLiteral("Bibliothek: %1").arg(problem));
        }
    }

    // Port nach außen führen / ausblenden (Rechtsklick auf einen Port in der Gruppe)
    {
        const graph::PortRef extra{*a, "out"};
        if (!setGroupPortExposed(group.name, extra, "Bild", true) || player_.project()->graph.findGroup(group.name)->ports.size() != 2 ||
            !setGroupPortExposed(group.name, extra, "Bild", false) || player_.project()->graph.findGroup(group.name)->ports.size() != 1) {
            return giveUp(QStringLiteral("Port nach außen führen / ausblenden"));
        }
    }

    scene_.clearSelection();
    if (itemOf(*a) == nullptr) {
        return giveUp(QStringLiteral("Kachel fehlt nach dem zweiten Gruppieren"));
    }
    itemOf(*a)->setSelected(true);
    removeSelected();
    if (!player_.project()->graph.groups().empty() || player_.project()->graph.findNode(*a) != nullptr ||
        player_.project()->graph.findNode(*b) != nullptr) {
        return giveUp(QStringLiteral("Entf auf der Kachel hat Gruppe und Nodes nicht gelöscht"));
    }

    // Dialog: ohne Namen oder mit vergebenem Namen sagt er, warum nicht
    GroupDialog dialog{QStringLiteral("Test"), QString{}, QColor{64, 140, 255}, {}, {QStringLiteral("Belegt")}, this};
    if (dialog.problem().isEmpty()) {
        return giveUp(QStringLiteral("der Dialog nimmt einen leeren Namen an"));
    }
    cleanup();
    return {};
}

}  // namespace whitehole::studio
