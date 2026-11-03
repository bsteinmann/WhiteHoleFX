// node_docking.cpp: Umsetzung zu node_docking.hpp. Wählt die Ports, an denen ein neuer Node einrastet.
#include "whitehole/graph/node_docking.hpp"

namespace whitehole::graph {

namespace {

const PortDescriptor* portOf(const Graph& graph, const PortRef& ref) {
    const NodeInstance* node = graph.findNode(ref.node);
    const NodeDescriptor* descriptor = node != nullptr ? graph.registry().find(node->typeId) : nullptr;
    return descriptor != nullptr ? descriptor->findPort(ref.portKey) : nullptr;
}

/// Der Eingang des neuen Nodes für einen Ausgang vom Typ `from`: erst gleicher Typ, dann irgendein passender.
const PortDescriptor* pickInput(const NodeDescriptor& node, PortType from) {
    const PortDescriptor* compatible = nullptr;
    for (const PortDescriptor& port : node.ports) {
        if (port.direction != PortDirection::Input || !isConnectable(from, port.type)) {
            continue;
        }
        if (port.type == from) {
            return &port;
        }
        compatible = compatible != nullptr ? compatible : &port;
    }
    return compatible;
}

/// Der Ausgang des neuen Nodes für einen Eingang vom Typ `to`: erst gleicher Typ, dann irgendein passender.
const PortDescriptor* pickOutput(const NodeDescriptor& node, PortType to) {
    const PortDescriptor* compatible = nullptr;
    for (const PortDescriptor& port : node.ports) {
        if (port.direction != PortDirection::Output || !isConnectable(port.type, to)) {
            continue;
        }
        if (port.type == to) {
            return &port;
        }
        compatible = compatible != nullptr ? compatible : &port;
    }
    return compatible;
}

std::optional<DockPlan> planWire(const Graph& graph, const NodeDescriptor& node, const Connection& wire) {
    const PortDescriptor* source = portOf(graph, wire.from);
    const PortDescriptor* sink = portOf(graph, wire.to);
    if (source == nullptr || sink == nullptr) {
        return std::nullopt;
    }
    const PortDescriptor* in = pickInput(node, source->type);
    const PortDescriptor* out = pickOutput(node, sink->type);
    if (in == nullptr || out == nullptr) {
        return std::nullopt;
    }
    DockPlan plan;
    plan.remove.push_back(wire.id);
    plan.links.push_back({wire.from, PortRef{NodeId{}, in->key}});
    plan.links.push_back({PortRef{NodeId{}, out->key}, wire.to});
    return plan;
}

/// Links vom Zielnode: Der neue Node speist einen Eingang des Zielnodes.
std::optional<DockPlan> planFeeding(const Graph& graph, const NodeDescriptor& node, const NodeInstance& host,
                                    const NodeDescriptor& hostType, const std::string& preferred) {
    const PortDescriptor* chosen = nullptr;
    const PortDescriptor* chosenOut = nullptr;
    const auto consider = [&](const PortDescriptor& port) {
        const PortDescriptor* out = pickOutput(node, port.type);
        if (port.direction == PortDirection::Input && out != nullptr) {
            chosen = &port;
            chosenOut = out;
        }
    };
    if (!preferred.empty()) {
        if (const PortDescriptor* port = hostType.findPort(preferred)) {
            consider(*port);
        }
    }
    // Reihenfolge: frei und gleicher Typ, frei und passend, belegt und gleicher Typ, belegt und passend.
    for (int pass = 0; chosen == nullptr && pass < 4; ++pass) {
        const bool wantFree = pass < 2;
        const bool wantExact = pass % 2 == 0;
        for (const PortDescriptor& port : hostType.ports) {
            const PortDescriptor* out = port.direction == PortDirection::Input ? pickOutput(node, port.type) : nullptr;
            if (out == nullptr) {
                continue;
            }
            const bool free = graph.findInputConnection(host.id, port.key) == nullptr;
            if (free == wantFree && (out->type == port.type) == wantExact) {
                chosen = &port;
                chosenOut = out;
                break;
            }
        }
    }
    if (chosen == nullptr) {
        return std::nullopt;
    }
    DockPlan plan;
    if (const Connection* existing = graph.findInputConnection(host.id, chosen->key)) {
        // Belegt: einfügen statt ersetzen. Passt der bisherige Ausgang nicht in den neuen Node, bleibt alles wie es ist.
        const PortDescriptor* previous = portOf(graph, existing->from);
        const PortDescriptor* in = previous != nullptr ? pickInput(node, previous->type) : nullptr;
        if (in == nullptr) {
            return std::nullopt;
        }
        plan.remove.push_back(existing->id);
        plan.links.push_back({existing->from, PortRef{NodeId{}, in->key}});
    }
    plan.links.push_back({PortRef{NodeId{}, chosenOut->key}, PortRef{host.id, chosen->key}});
    return plan;
}

/// Rechts vom Zielnode: Ein Ausgang des Zielnodes speist den neuen Node.
std::optional<DockPlan> planAppending(const NodeDescriptor& node, const NodeInstance& host, const NodeDescriptor& hostType,
                                      const std::string& preferred) {
    const PortDescriptor* chosen = nullptr;
    const PortDescriptor* chosenIn = nullptr;
    const auto consider = [&](const PortDescriptor& port) {
        const PortDescriptor* in = port.direction == PortDirection::Output ? pickInput(node, port.type) : nullptr;
        if (in != nullptr) {
            chosen = &port;
            chosenIn = in;
        }
    };
    if (!preferred.empty()) {
        if (const PortDescriptor* port = hostType.findPort(preferred)) {
            consider(*port);
        }
    }
    for (const PortDescriptor& port : hostType.ports) {
        if (chosen != nullptr) {
            break;
        }
        consider(port);
    }
    if (chosen == nullptr) {
        return std::nullopt;
    }
    DockPlan plan;
    plan.links.push_back({PortRef{host.id, chosen->key}, PortRef{NodeId{}, chosenIn->key}});
    return plan;
}

}  // namespace

std::optional<DockPlan> planDocking(const Graph& graph, const NodeDescriptor& newNode, const DockTarget& target) {
    if (target.kind == DockTarget::Kind::Wire) {
        const Connection* wire = graph.findConnection(target.wire);
        return wire != nullptr ? planWire(graph, newNode, *wire) : std::nullopt;
    }
    const NodeInstance* host = graph.findNode(target.node);
    const NodeDescriptor* hostType = host != nullptr ? graph.registry().find(host->typeId) : nullptr;
    if (host == nullptr || hostType == nullptr) {
        return std::nullopt;
    }
    return target.rightHalf ? planAppending(newNode, *host, *hostType, target.preferredPort)
                            : planFeeding(graph, newNode, *host, *hostType, target.preferredPort);
}

}  // namespace whitehole::graph
