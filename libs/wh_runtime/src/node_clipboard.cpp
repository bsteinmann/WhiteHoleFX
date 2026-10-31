// node_clipboard.cpp: Nodes aus dem Graphen in die Zwischenablage kopieren.
#include "whitehole/runtime/node_clipboard.hpp"

#include <algorithm>
#include <unordered_map>

namespace whitehole::runtime {

Vec2 NodeClipboard::topLeft() const noexcept {
    if (nodes.empty()) {
        return Vec2{};
    }
    Vec2 corner = nodes.front().position;
    for (const ClipboardNode& node : nodes) {
        corner.x = std::min(corner.x, node.position.x);
        corner.y = std::min(corner.y, node.position.y);
    }
    return corner;
}

NodeClipboard copyNodes(const graph::Graph& graph, std::span<const NodeId> selection) {
    NodeClipboard clip;
    std::unordered_map<std::uint64_t, std::size_t> index;  // NodeId -> Platz in clip.nodes
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (std::ranges::find(selection, node.id) == selection.end()) {
            continue;
        }
        ClipboardNode copy;
        copy.typeId = node.typeId;
        copy.position = node.position;
        for (const auto& [key, value] : node.parameters) {
            copy.parameters.emplace_back(key, value);
        }
        std::ranges::sort(copy.parameters, {}, &std::pair<std::string, graph::ParameterValue>::first);
        index.emplace(node.id.value(), clip.nodes.size());
        clip.nodes.push_back(std::move(copy));
    }
    for (const graph::Connection& c : graph.connections()) {
        const auto from = index.find(c.from.node.value());
        const auto to = index.find(c.to.node.value());
        if (from != index.end() && to != index.end()) {
            clip.wires.push_back(ClipboardWire{from->second, c.from.portKey, to->second, c.to.portKey});
        }
    }
    return clip;
}

Result<std::vector<NodeId>> applyClipboard(graph::Graph& graph, const NodeClipboard& clipboard, Vec2 offset) {
    std::vector<NodeId> ids;
    ids.reserve(clipboard.nodes.size());
    for (const ClipboardNode& node : clipboard.nodes) {
        const auto id = graph.addNode(node.typeId, Vec2{node.position.x + offset.x, node.position.y + offset.y});
        if (!id.ok()) {
            return id.error();
        }
        ids.push_back(id.value());
        for (const auto& [key, value] : node.parameters) {
            if (const auto set = graph.setParameter(id.value(), key, value); !set.ok()) {
                return set.error();
            }
        }
    }
    for (const ClipboardWire& wire : clipboard.wires) {
        if (wire.from >= ids.size() || wire.to >= ids.size()) {
            return Error{ErrorCode::InvalidArgument, "Zwischenablage ist beschädigt"};
        }
        const auto connected = graph.connect(graph::PortRef{ids[wire.from], wire.fromPort},
                                             graph::PortRef{ids[wire.to], wire.toPort});
        if (!connected.ok()) {
            return connected.error();
        }
    }
    return ids;
}

}  // namespace whitehole::runtime
