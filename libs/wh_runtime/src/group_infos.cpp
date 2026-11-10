// group_infos.cpp: Parameter von Gruppen für die Parameteranzeige (siehe group_infos.hpp).
#include "whitehole/runtime/group_infos.hpp"

#include <algorithm>
#include <charconv>

namespace whitehole::runtime {

std::string groupParameterKey(NodeId node, const std::string& key) {
    return "@" + std::to_string(node.value()) + "/" + key;
}

std::optional<std::pair<NodeId, std::string>> parseGroupParameterKey(std::string_view key) {
    if (key.size() < 4 || key.front() != '@') {
        return std::nullopt;
    }
    const std::size_t slash = key.find('/');
    if (slash == std::string_view::npos || slash == 1 || slash + 1 >= key.size()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const auto digits = key.substr(1, slash - 1);
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return std::pair{NodeId{value}, std::string{key.substr(slash + 1)}};
}

std::vector<NodeInfo> applyGroups(const graph::Graph& graph, std::vector<NodeInfo> nodes) {
    std::vector<NodeInfo> result;
    result.reserve(nodes.size());
    for (NodeInfo& node : nodes) {
        const graph::NodeGroup* group = graph.groupOf(node.id);
        if (group == nullptr || !group->collapsed) {
            result.push_back(std::move(node));
            continue;
        }
        if (group->members.front() != node.id) {
            continue;  // die übrigen Mitglieder verschwinden hinter dem Eintrag der Gruppe
        }
        NodeInfo entry;
        entry.id = node.id;
        entry.title = group->name;
        entry.typeId = "group";
        for (const graph::GroupParameter& wanted : group->parameters) {
            const auto member = std::ranges::find(nodes, wanted.node, &NodeInfo::id);
            if (member == nodes.end()) {
                continue;
            }
            const auto parameter = std::ranges::find(member->parameters, wanted.key, &ParameterInfo::key);
            if (parameter == member->parameters.end()) {
                continue;
            }
            ParameterInfo copy = *parameter;
            copy.key = groupParameterKey(wanted.node, wanted.key);
            copy.displayName = wanted.label;
            entry.parameters.push_back(std::move(copy));
        }
        result.push_back(std::move(entry));
    }
    return result;
}

}  // namespace whitehole::runtime
