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
            clip.wires.push_back(ClipboardWire{from->second, c.from.portKey, to->second, c.to.portKey, c.scale, c.offset});
        }
    }
    for (const graph::NodeGroup& group : graph.groups()) {
        ClipboardGroup copy;
        copy.name = group.name;
        copy.position = group.position;
        copy.color = group.color;
        copy.collapsed = group.collapsed;
        bool complete = !group.members.empty();
        for (const NodeId member : group.members) {
            const auto it = index.find(member.value());
            complete = complete && it != index.end();
            if (it != index.end()) {
                copy.members.push_back(it->second);
            }
        }
        if (!complete) {
            continue;
        }
        for (const graph::GroupPort& port : group.ports) {
            copy.ports.push_back({index.at(port.node.value()), port.portKey, port.label});
        }
        for (const graph::GroupParameter& parameter : group.parameters) {
            copy.parameters.push_back({index.at(parameter.node.value()), parameter.key, parameter.label});
        }
        clip.groups.push_back(std::move(copy));
    }
    return clip;
}

namespace {

/// Ein Gruppenname, der im Graphen noch frei ist: "Name", sonst "Name 2", "Name 3" ...
std::string freeGroupName(const graph::Graph& graph, const std::string& wanted) {
    if (graph.findGroup(wanted) == nullptr) {
        return wanted;
    }
    for (int n = 2;; ++n) {
        const std::string suffix = " " + std::to_string(n);
        const std::string base = wanted.substr(0, graph::kMaxGroupNameLength - suffix.size());
        if (graph.findGroup(base + suffix) == nullptr) {
            return base + suffix;
        }
    }
}

}  // namespace

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
        if (wire.scale != 1.0 || wire.offset != 0.0) {
            if (const auto scaled = graph.setConnectionScale(connected.value(), wire.scale, wire.offset); !scaled.ok()) {
                return scaled.error();
            }
        }
    }
    for (const ClipboardGroup& source : clipboard.groups) {
        graph::NodeGroup group;
        group.name = freeGroupName(graph, source.name);
        group.position = Vec2{source.position.x + offset.x, source.position.y + offset.y};
        group.color = source.color;
        group.collapsed = source.collapsed;
        const auto idOf = [&](std::size_t index) -> Result<NodeId> {
            if (index >= ids.size()) {
                return Error{ErrorCode::InvalidArgument, "Zwischenablage ist beschädigt"};
            }
            return ids[index];
        };
        for (const std::size_t member : source.members) {
            const auto id = idOf(member);
            if (!id.ok()) {
                return id.error();
            }
            group.members.push_back(id.value());
        }
        for (const ClipboardGroup::Port& port : source.ports) {
            const auto id = idOf(port.node);
            if (!id.ok()) {
                return id.error();
            }
            group.ports.push_back(graph::GroupPort{id.value(), port.key, port.label});
        }
        for (const ClipboardGroup::Port& parameter : source.parameters) {
            const auto id = idOf(parameter.node);
            if (!id.ok()) {
                return id.error();
            }
            group.parameters.push_back(graph::GroupParameter{id.value(), parameter.key, parameter.label});
        }
        if (const auto added = graph.addGroup(std::move(group)); !added.ok()) {
            return added.error();
        }
    }
    return ids;
}

}  // namespace whitehole::runtime
