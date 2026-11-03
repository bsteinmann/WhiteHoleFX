// player_dock.cpp: Node anlegen und gleich verdrahten (Teil des Players, ausgelagert wegen der Dateigröße).
//
// Warum eine eigene Funktion: Zieht man einen Node aus der Palette auf ein Kabel oder einen Node, entstehen Node
// und Kabel in einem Zug. Ein Rückgängig soll alles zusammen zurücknehmen, nicht erst die Kabel und dann den Node.
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

Result<NodeId> Player::addNodeDocked(std::string_view typeId, Vec2 position, const graph::DockPlan& plan,
                                     const std::vector<graph::NodeMove>& moves) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (typeId == "wh.output.canvas") {
        return Error{ErrorCode::InvalidArgument, "Der Ausgangs-Node rastet nicht ein"};
    }
    const project::Project backup = *project_;
    auto before = beginEdit({});
    graph::Graph& g = project_->graph;
    const auto fail = [&](Error error) -> Result<NodeId> {
        *project_ = backup;  // alles oder nichts
        return error;
    };

    const Result<NodeId> created = g.addNode(typeId, position);
    if (!created.ok()) {
        return fail(created.error());
    }
    const NodeId id = created.value();
    if (const NodeDefaults::Values* own = nodeDefaults_.valuesOf(std::string{typeId})) {
        for (const auto& [key, value] : *own) {
            (void)g.setParameter(id, key, value);
        }
    }
    for (const graph::NodeMove& move : moves) {
        if (const Result<void> r = g.setPosition(move.first, move.second); !r.ok()) {
            return fail(r.error());
        }
    }
    for (const ConnectionId wire : plan.remove) {
        if (const Result<void> r = g.disconnect(wire); !r.ok()) {
            return fail(r.error());
        }
    }
    for (const graph::DockLink& link : plan.links) {
        graph::PortRef from = link.from;
        graph::PortRef to = link.to;
        (from.node.valid() ? to : from).node = id;  // der Platzhalter steht für den neuen Node
        if (const Result<ConnectionId> r = g.connect(from, to); !r.ok()) {
            return fail(r.error());
        }
    }
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return id;
}

}  // namespace whitehole::runtime
