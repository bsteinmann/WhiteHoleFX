// node_editor_library.cpp: Bibliothek im Node-Editor (Gruppen speichern, Einträge einfügen) und das Port-Menü der Gruppen.
//
// Eine Gruppe in der Bibliothek ist eine Preset-Datei (runtime/node_preset.hpp), die auch den `nodegroup`-Block enthält,
// in einem eigenen Ordner. Eingefügt wird sie wie ein Preset (ein Schritt Rückgängig), als zugeklappte Gruppe.
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>

#include <algorithm>

#include "node_editor.hpp"
#include "node_palette.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }
std::string toStd(const QString& text) { return text.toUtf8().toStdString(); }

}  // namespace

bool NodeEditor::insertFromStore(const runtime::PresetStore* store, const QString& name, const QPointF& scenePos,
                                 bool collapseGroups) {
    if (store == nullptr || player_.project() == nullptr) {
        return false;
    }
    const auto text = store->load(toStd(name));
    if (!text.ok()) {
        fail(text.error().message);
        return false;
    }
    auto clip = runtime::presetFromText(text.value(), player_.registry());
    if (!clip.ok()) {
        fail("\"" + toStd(name) + "\": " + clip.error().message);
        return false;
    }
    if (collapseGroups) {
        for (runtime::ClipboardGroup& group : clip.value().groups) {
            group.collapsed = true;
        }
    }
    const Vec2 corner = clip.value().topLeft();
    return pasteWithOffset(clip.value(), scenePos - QPointF{static_cast<qreal>(corner.x), static_cast<qreal>(corner.y)});
}

bool NodeEditor::saveGroupToLibrary(const std::string& groupName, bool askReplace) {
    const graph::NodeGroup* group = player_.project() != nullptr ? player_.project()->graph.findGroup(groupName) : nullptr;
    if (group == nullptr || groups_ == nullptr) {
        return false;
    }
    if (!runtime::isValidPresetName(groupName)) {
        fail("Der Gruppenname taugt nicht als Dateiname (keine Sonderzeichen wie / \\ : * ? \" < > |)");
        return false;
    }
    if (askReplace && groups_->contains(groupName) &&
        QMessageBox::question(this, tr("Gruppe ersetzen"), tr("Die Bibliothek enthält schon eine Gruppe \"%1\". Ersetzen?").arg(fromStd(groupName)),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return false;
    }
    const runtime::NodeClipboard clip = runtime::copyNodes(player_.project()->graph, group->members);
    const auto text = runtime::presetToText(clip, player_.registry());
    if (!text.ok()) {
        fail(text.error().message);
        return false;
    }
    if (const auto saved = groups_->save(groupName, text.value()); !saved.ok()) {
        fail(saved.error().message);
        return false;
    }
    Q_EMIT notice(tr("Gruppe \"%1\" in der Bibliothek gespeichert.").arg(fromStd(groupName)));
    Q_EMIT libraryChanged();
    return true;
}

bool NodeEditor::insertLibraryItem(const QString& kind, const QString& name, const QPointF& scenePos) {
    if (kind == QStringLiteral("group")) {
        return insertFromStore(groups_, name, scenePos, true);
    }
    return insertFromStore(presets_, name, scenePos, false);
}

bool NodeEditor::handleLibraryDrop(const QMimeData* mime, const QPoint& viewportPos) {
    if (mime == nullptr || !mime->hasFormat(QString::fromLatin1(kLibraryMime))) {
        return false;
    }
    const QStringList parts = QString::fromUtf8(mime->data(QString::fromLatin1(kLibraryMime))).split(QLatin1Char('\n'));
    if (parts.size() != 2) {
        return false;
    }
    if (player_.project() == nullptr) {
        Q_EMIT notice(tr("Zuerst ein Projekt anlegen (Strg+N) oder öffnen (Strg+O)."));
        return true;
    }
    fitPending_ = false;
    const QRect area = viewport()->rect();
    const QPoint inside{std::clamp(viewportPos.x(), 0, std::max(0, area.width() - 1)),
                        std::clamp(viewportPos.y(), 0, std::max(0, area.height() - 1))};
    insertLibraryItem(parts[0], parts[1], mapToScene(inside));
    ensureNodesVisible();
    return true;
}

// --- Port einer Gruppe nach außen führen oder ausblenden ---------------------------------------

bool NodeEditor::setGroupPortExposed(const std::string& groupName, const graph::PortRef& port, const std::string& label,
                                     bool exposed) {
    const graph::NodeGroup* group = player_.project() != nullptr ? player_.project()->graph.findGroup(groupName) : nullptr;
    if (group == nullptr) {
        return false;
    }
    graph::NodeGroup changed = *group;
    std::erase_if(changed.ports, [&](const graph::GroupPort& p) { return p.node == port.node && p.portKey == port.portKey; });
    if (exposed) {
        changed.ports.push_back(graph::GroupPort{port.node, port.portKey, label});
    }
    if (const auto r = player_.replaceNodeGroup(groupName, changed); !r.ok()) {
        fail(r.error().message);
        return false;
    }
    rebuild();
    Q_EMIT edited(true);
    return true;
}

void NodeEditor::addGroupPortMenu(QMenu& menu, const Hit& hit) {
    if (player_.project() == nullptr || hit.node == nullptr || hit.port == nullptr) {
        return;
    }
    const graph::Graph& g = player_.project()->graph;
    const graph::PortRef ref = hit.node->portRef(*hit.port);
    const graph::NodeGroup* group = g.groupOf(ref.node);
    if (group == nullptr) {
        return;
    }
    const bool exposed = std::ranges::any_of(
        group->ports, [&](const graph::GroupPort& p) { return p.node == ref.node && p.portKey == ref.portKey; });
    // Mit Kabel nach außen bleibt ein Anschluss auf der Kachel sichtbar, sonst wäre das Kabel unsichtbar.
    bool connectedOutside = false;
    for (const graph::Connection& c : g.connections()) {
        const graph::PortRef& mine = hit.port->output ? c.from : c.to;
        const graph::PortRef& other = hit.port->output ? c.to : c.from;
        if (mine.node == ref.node && mine.portKey == ref.portKey &&
            std::ranges::find(group->members, other.node) == group->members.end()) {
            connectedOutside = true;
        }
    }
    const std::string groupName = group->name;
    const std::string label = toStd(hit.port->label);
    menu.addSeparator();
    if (exposed) {
        QAction* hide = menu.addAction(connectedOutside ? tr("Auf der Kachel ausblenden (Kabel nach außen vorhanden)")
                                                        : tr("Auf der Kachel ausblenden"));
        hide->setEnabled(!connectedOutside);
        connect(hide, &QAction::triggered, this, [this, groupName, ref, label]() { setGroupPortExposed(groupName, ref, label, false); });
    } else {
        connect(menu.addAction(tr("Nach außen führen (auf der Kachel zeigen)")), &QAction::triggered, this,
                [this, groupName, ref, label]() { setGroupPortExposed(groupName, ref, label, true); });
    }
}

}  // namespace whitehole::studio
