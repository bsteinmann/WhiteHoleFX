// project_nodegroup.cpp: Umsetzung zu project_nodegroup.hpp.
#include "project_nodegroup.hpp"

namespace whitehole::project::detail {

namespace {

/// "<node>.<key>" zerlegen; sonst Fehler im Reader.
std::pair<std::string, std::string> splitRef(Reader& r, const char* what) {
    const std::string text = r.word(what);
    const std::size_t dot = text.find('.');
    if (r.ok() && (dot == std::string::npos || dot == 0 || dot + 1 == text.size())) {
        r.fail(std::string{what} + ": erwartet <node>.<name>, gefunden \"" + text + "\"");
        return {};
    }
    return r.ok() ? std::pair{text.substr(0, dot), text.substr(dot + 1)} : std::pair<std::string, std::string>{};
}

NodeGroupDraft::Ref readRef(Reader& r, const char* what) {
    auto [node, key] = splitRef(r, what);
    const std::string as = r.word("\"as\"");
    if (r.ok() && as != "as") {
        r.fail(std::string{"Nach "} + what + " muss \"as\" und die Beschriftung folgen");
    }
    std::string label = r.word("Beschriftung");
    r.expectEnd();
    return {std::move(node), std::move(key), std::move(label)};
}

}  // namespace

NodeGroupDraft beginNodeGroup(Reader& r) {
    NodeGroupDraft draft;
    draft.name = r.word("Gruppenname");
    r.expectEnd();
    return draft;
}

void nodeGroupStatement(std::string_view key, Reader& r, NodeGroupDraft& draft) {
    if (key == "at") {
        draft.position = r.vec2("Position");
        r.expectEnd();
    } else if (key == "color") {
        draft.color = r.vec3("Farbe");
        r.expectEnd();
    } else if (key == "expanded") {
        draft.collapsed = false;
        r.expectEnd();
    } else if (key == "member") {
        do {
            draft.members.push_back(r.word("Nodename"));
        } while (r.ok() && !r.atEnd());
    } else if (key == "port") {
        draft.ports.push_back(readRef(r, "Port"));
    } else if (key == "param") {
        draft.parameters.push_back(readRef(r, "Parameter"));
    } else {
        r.fail("Unbekannte Anweisung \"" + std::string{key} + "\" im nodegroup-Block");
    }
}

Result<void> finishNodeGroup(const NodeGroupDraft& draft, const std::unordered_map<std::string, NodeId>& nodes,
                             graph::Graph& graph) {
    const auto find = [&](const std::string& name) -> Result<NodeId> {
        const auto it = nodes.find(name);
        if (it == nodes.end()) {
            return Error{ErrorCode::NotFound, "Unbekannter Node \"" + name + "\""};
        }
        return it->second;
    };
    graph::NodeGroup group;
    group.name = draft.name;
    group.position = draft.position;
    group.color = draft.color;
    group.collapsed = draft.collapsed;
    for (const std::string& member : draft.members) {
        const auto id = find(member);
        if (!id.ok()) {
            return id.error();
        }
        group.members.push_back(id.value());
    }
    for (const NodeGroupDraft::Ref& ref : draft.ports) {
        const auto id = find(ref.node);
        if (!id.ok()) {
            return id.error();
        }
        group.ports.push_back({id.value(), ref.key, ref.label});
    }
    for (const NodeGroupDraft::Ref& ref : draft.parameters) {
        const auto id = find(ref.node);
        if (!id.ok()) {
            return id.error();
        }
        group.parameters.push_back({id.value(), ref.key, ref.label});
    }
    return graph.addGroup(std::move(group));
}

void writeNodeGroups(std::ostream& out, const graph::Graph& graph,
                     const std::unordered_map<std::uint64_t, std::string>& names) {
    for (const graph::NodeGroup& group : graph.groups()) {
        out << "nodegroup " << quoteWord(group.name) << "\n";
        out << "  at " << formatFloat(group.position.x) << " " << formatFloat(group.position.y) << "\n";
        out << "  color " << formatFloat(group.color.x) << " " << formatFloat(group.color.y) << " "
            << formatFloat(group.color.z) << "\n";
        if (!group.collapsed) {
            out << "  expanded\n";
        }
        out << "  member";
        for (const NodeId id : group.members) {
            out << " " << names.at(id.value());
        }
        out << "\n";
        for (const graph::GroupPort& port : group.ports) {
            out << "  port " << names.at(port.node.value()) << "." << port.portKey << " as " << quoteWord(port.label) << "\n";
        }
        for (const graph::GroupParameter& parameter : group.parameters) {
            out << "  param " << names.at(parameter.node.value()) << "." << parameter.key << " as "
                << quoteWord(parameter.label) << "\n";
        }
        out << "end\n\n";
    }
}

}  // namespace whitehole::project::detail
