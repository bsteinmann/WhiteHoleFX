// player_paste.cpp: Nodes aus der Zwischenablage einfügen (Teil des Players).
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

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
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return ids;
}

}  // namespace whitehole::runtime
