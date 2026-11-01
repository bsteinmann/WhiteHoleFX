// player_paste.cpp: Nodes aus der Zwischenablage einfügen (Teil des Players).
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

/// Der letzte eingefügte Ausgangs-Node, dessen Ausgang noch nirgends hinführt (die Spitze einer eingefügten Kette).
std::optional<NodeId> pastedOutput(const graph::Graph& graph, const std::vector<NodeId>& ids) {
    std::optional<NodeId> found;
    for (const NodeId id : ids) {
        const graph::NodeInstance* node = graph.findNode(id);
        if (node == nullptr || node->typeId != "wh.output.canvas") {
            continue;
        }
        bool used = false;
        for (const graph::Connection& wire : graph.connections()) {
            used = used || wire.from.node == id;
        }
        if (!used) {
            found = id;
        }
    }
    return found;
}

}  // namespace

Result<std::vector<NodeId>> Player::pasteNodes(const NodeClipboard& clipboard, Vec2 offset) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (clipboard.empty()) {
        return Error{ErrorCode::InvalidArgument, "Die Zwischenablage ist leer"};
    }
    // Alles oder nichts: Scheitert ein Schritt, kehrt das Projekt zum alten Stand zurück.
    auto before = beginEdit({});
    const project::Project backup = *project_;
    auto ids = applyClipboard(project_->graph, clipboard, offset);
    if (!ids.ok()) {
        *project_ = backup;  // alles oder nichts
        return ids.error();
    }
    // Ein eingefügter Ausgangs-Node (Preset, Kopie) soll wie ein neu angelegter wirken: Er wird zum Ausgang des
    // Projekts und legt sich über den bisherigen. Ohne das meldet das Projekt "kein Ausgang", obwohl ein Output da ist.
    if (const std::optional<NodeId> output = pastedOutput(project_->graph, ids.value())) {
        if (project_->target) {
            if (project_->graph.findInputConnection(*output, "source") == nullptr) {
                (void)project_->graph.connect(*project_->target, graph::PortRef{*output, "source"});
            }
            if (project_->graph.findInputConnection(*output, "behind") == nullptr) {
                (void)project_->graph.connect(*project_->target, graph::PortRef{*output, "behind"});
            }
        }
        project_->target = graph::PortRef{*output, "out"};
        rebuildDirty_ = true;
    }
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return ids;
}

}  // namespace whitehole::runtime
