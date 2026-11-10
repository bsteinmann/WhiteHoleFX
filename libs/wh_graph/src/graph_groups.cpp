// graph_groups.cpp: Gruppen im Graphen anlegen, ändern, auflösen und prüfen (siehe node_group.hpp).
#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "whitehole/graph/graph_model.hpp"

namespace whitehole::graph {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidArgument, std::move(message)}; }
Error missing(std::string message) { return Error{ErrorCode::NotFound, std::move(message)}; }

}  // namespace

Result<void> Graph::checkGroup(const NodeGroup& group, std::string_view replacing) const {
    if (group.name.empty() || group.name.size() > kMaxGroupNameLength) {
        return invalid("Der Name der Gruppe muss 1 bis " + std::to_string(kMaxGroupNameLength) + " Zeichen haben");
    }
    for (const NodeGroup& other : groups_) {
        if (other.name == group.name && other.name != replacing) {
            return Error{ErrorCode::AlreadyExists, "Eine Gruppe heißt schon \"" + group.name + "\""};
        }
    }
    if (group.members.empty()) {
        return invalid("Eine Gruppe braucht mindestens einen Node");
    }
    std::set<std::uint64_t> members;
    for (const NodeId id : group.members) {
        if (findNode(id) == nullptr) {
            return missing("Ein Node der Gruppe existiert nicht");
        }
        if (!members.insert(id.value()).second) {
            return invalid("Ein Node steht doppelt in der Gruppe");
        }
        const NodeGroup* owner = groupOf(id);
        if (owner != nullptr && owner->name != replacing) {
            return Error{ErrorCode::AlreadyExists, "Der Node steckt schon in der Gruppe \"" + owner->name + "\""};
        }
    }
    std::set<std::pair<std::uint64_t, std::string>> seen;
    for (const GroupPort& port : group.ports) {
        const NodeInstance* node = members.contains(port.node.value()) ? findNode(port.node) : nullptr;
        if (node == nullptr) {
            return invalid("Ein Port gehört zu keinem Node der Gruppe");
        }
        const NodeDescriptor* descriptor = registry_->find(node->typeId);
        if (descriptor == nullptr || descriptor->findPort(port.portKey) == nullptr) {
            return missing("Port existiert nicht: " + port.portKey);
        }
        if (port.label.empty()) {
            return invalid("Die Beschriftung eines Ports darf nicht leer sein");
        }
        if (!seen.emplace(port.node.value(), port.portKey).second) {
            return invalid("Der Port " + port.portKey + " steht doppelt in der Schnittstelle");
        }
    }
    seen.clear();
    for (const GroupParameter& parameter : group.parameters) {
        const NodeInstance* node = members.contains(parameter.node.value()) ? findNode(parameter.node) : nullptr;
        if (node == nullptr) {
            return invalid("Ein Parameter gehört zu keinem Node der Gruppe");
        }
        const NodeDescriptor* descriptor = registry_->find(node->typeId);
        const bool known = descriptor != nullptr &&
                           std::any_of(descriptor->parameters.begin(), descriptor->parameters.end(),
                                       [&](const ParameterDescriptor& p) { return p.key == parameter.key; });
        if (!known) {
            return missing("Parameter existiert nicht: " + parameter.key);
        }
        if (parameter.label.empty()) {
            return invalid("Die Beschriftung eines Parameters darf nicht leer sein");
        }
        if (!seen.emplace(parameter.node.value(), parameter.key).second) {
            return invalid("Der Parameter " + parameter.key + " steht doppelt in der Schnittstelle");
        }
    }
    if (!std::isfinite(group.position.x) || !std::isfinite(group.position.y)) {
        return invalid("Die Lage der Gruppe muss eine endliche Zahl sein");
    }
    return {};
}

Result<void> Graph::addGroup(NodeGroup group) {
    if (auto checked = checkGroup(group, {}); !checked.ok()) {
        return checked;
    }
    groups_.push_back(std::move(group));
    return {};
}

Result<void> Graph::replaceGroup(std::string_view name, NodeGroup group) {
    const auto it = std::find_if(groups_.begin(), groups_.end(), [&](const NodeGroup& g) { return g.name == name; });
    if (it == groups_.end()) {
        return missing("Gruppe existiert nicht");
    }
    if (auto checked = checkGroup(group, name); !checked.ok()) {
        return checked;
    }
    *it = std::move(group);
    return {};
}

Result<void> Graph::removeGroup(std::string_view name) {
    const auto removed = std::erase_if(groups_, [&](const NodeGroup& g) { return g.name == name; });
    return removed > 0 ? Result<void>{} : Result<void>{missing("Gruppe existiert nicht")};
}

Result<void> Graph::setGroupCollapsed(std::string_view name, bool collapsed) {
    const auto it = std::find_if(groups_.begin(), groups_.end(), [&](const NodeGroup& g) { return g.name == name; });
    if (it == groups_.end()) {
        return missing("Gruppe existiert nicht");
    }
    it->collapsed = collapsed;
    return {};
}

Result<void> Graph::setGroupPosition(std::string_view name, Vec2 position) {
    const auto it = std::find_if(groups_.begin(), groups_.end(), [&](const NodeGroup& g) { return g.name == name; });
    if (it == groups_.end()) {
        return missing("Gruppe existiert nicht");
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        return invalid("Die Lage der Gruppe muss eine endliche Zahl sein");
    }
    it->position = position;
    return {};
}

const NodeGroup* Graph::findGroup(std::string_view name) const noexcept {
    const auto it = std::find_if(groups_.begin(), groups_.end(), [&](const NodeGroup& g) { return g.name == name; });
    return it != groups_.end() ? &*it : nullptr;
}

const NodeGroup* Graph::groupOf(NodeId node) const noexcept {
    for (const NodeGroup& group : groups_) {
        if (std::find(group.members.begin(), group.members.end(), node) != group.members.end()) {
            return &group;
        }
    }
    return nullptr;
}

void Graph::dropFromGroups(NodeId id) {
    for (NodeGroup& group : groups_) {
        std::erase(group.members, id);
        std::erase_if(group.ports, [id](const GroupPort& port) { return port.node == id; });
        std::erase_if(group.parameters, [id](const GroupParameter& parameter) { return parameter.node == id; });
    }
    std::erase_if(groups_, [](const NodeGroup& group) { return group.members.empty(); });
}

}  // namespace whitehole::graph
