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
    graph::Graph& g = project_->graph;

    const auto fail = [&](const Error& error) -> Result<std::vector<NodeId>> {
        *project_ = backup;
        return error;
    };

    std::vector<NodeId> ids;
    ids.reserve(clipboard.nodes.size());
    for (const ClipboardNode& node : clipboard.nodes) {
        const auto id = g.addNode(node.typeId, Vec2{node.position.x + offset.x, node.position.y + offset.y});
        if (!id.ok()) {
            return fail(id.error());
        }
        ids.push_back(id.value());
        for (const auto& [key, value] : node.parameters) {
            if (const auto set = g.setParameter(id.value(), key, value); !set.ok()) {
                return fail(set.error());
            }
        }
    }
    for (const ClipboardWire& wire : clipboard.wires) {
        if (wire.from >= ids.size() || wire.to >= ids.size()) {
            return fail(Error{ErrorCode::InvalidArgument, "Zwischenablage ist beschädigt"});
        }
        const auto connected = g.connect(graph::PortRef{ids[wire.from], wire.fromPort},
                                         graph::PortRef{ids[wire.to], wire.toPort});
        if (!connected.ok()) {
            return fail(connected.error());
        }
    }
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return ids;
}

}  // namespace whitehole::runtime
