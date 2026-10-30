#include "whitehole/graph/graph_topology.hpp"

#include <cstddef>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace whitehole::graph {

bool isReachable(std::span<const Connection> connections, NodeId from, NodeId to) {
    if (from == to) {
        return true;
    }

    std::unordered_map<NodeId, std::vector<NodeId>> downstream;
    for (const Connection& connection : connections) {
        downstream[connection.from.node].push_back(connection.to.node);
    }

    std::unordered_set<NodeId> visited{from};
    std::vector<NodeId> stack{from};
    while (!stack.empty()) {
        const NodeId current = stack.back();
        stack.pop_back();

        const auto it = downstream.find(current);
        if (it == downstream.end()) {
            continue;
        }
        for (const NodeId next : it->second) {
            if (next == to) {
                return true;
            }
            if (visited.insert(next).second) {
                stack.push_back(next);
            }
        }
    }
    return false;
}

Result<std::vector<NodeId>> topologicalOrder(std::span<const NodeId> nodes,
                                             std::span<const Connection> connections) {
    std::unordered_map<NodeId, std::size_t> indexOf;
    indexOf.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        indexOf.emplace(nodes[i], i);
    }

    std::vector<std::vector<std::size_t>> successors(nodes.size());
    std::vector<std::size_t> inDegree(nodes.size(), 0);
    for (const Connection& connection : connections) {
        const auto from = indexOf.find(connection.from.node);
        const auto to = indexOf.find(connection.to.node);
        if (from == indexOf.end() || to == indexOf.end()) {
            return Error{ErrorCode::NotFound, "Verbindung verweist auf einen unbekannten Node"};
        }
        successors[from->second].push_back(to->second);
        ++inDegree[to->second];
    }

    std::deque<std::size_t> ready;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (inDegree[i] == 0) {
            ready.push_back(i);
        }
    }

    std::vector<NodeId> order;
    order.reserve(nodes.size());
    while (!ready.empty()) {
        const std::size_t current = ready.front();
        ready.pop_front();
        order.push_back(nodes[current]);
        for (const std::size_t next : successors[current]) {
            if (--inDegree[next] == 0) {
                ready.push_back(next);
            }
        }
    }

    if (order.size() != nodes.size()) {
        return Error{ErrorCode::CycleDetected, "Der Graph enthält einen Zyklus"};
    }
    return order;
}

Result<std::vector<NodeId>> topologicalOrder(const Graph& graph) {
    std::vector<NodeId> ids;
    ids.reserve(graph.nodes().size());
    for (const NodeInstance& node : graph.nodes()) {
        ids.push_back(node.id);
    }
    return topologicalOrder(ids, graph.connections());
}

}  // namespace whitehole::graph
