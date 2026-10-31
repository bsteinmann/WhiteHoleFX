// value_evaluator.cpp: Umsetzung zu value_evaluator.hpp. Wertet die CPU-Nodes eines Graphen einmal pro Frame aus.
#include "whitehole/values/value_evaluator.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "whitehole/graph/graph_topology.hpp"
#include "whitehole/graph/parameter_resolve.hpp"

namespace whitehole::values {

namespace {

using graph::NodeDescriptor;
using graph::NodeInstance;
using graph::PortDirection;

std::string describe(const NodeInstance& node) {
    return "Node #" + std::to_string(node.id.value()) + " (" + node.typeId + ")";
}

bool hasOutputs(const NodeDescriptor& descriptor) {
    return std::any_of(descriptor.ports.begin(), descriptor.ports.end(),
                       [](const graph::PortDescriptor& port) {
                           return port.direction == PortDirection::Output;
                       });
}

/// Alle CPU-Nodes, deren Ergebnis jemand braucht (siehe Kopfkommentar).
std::unordered_set<NodeId> neededNodes(const graph::Graph& graph, const std::vector<NodeId>& order) {
    std::unordered_map<NodeId, std::vector<NodeId>> consumers;
    for (const graph::Connection& connection : graph.connections()) {
        consumers[connection.from.node].push_back(connection.to.node);
    }

    const auto isCpu = [&graph](NodeId id) {
        const NodeInstance* node = graph.findNode(id);
        const NodeDescriptor* descriptor = node == nullptr ? nullptr : graph.registry().find(node->typeId);
        return descriptor != nullptr && descriptor->domain == graph::NodeDomain::Cpu;
    };

    std::unordered_set<NodeId> needed;
    // Rückwärts durch die Reihenfolge: Alle Verbraucher eines Nodes sind schon entschieden.
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        const NodeInstance* node = graph.findNode(*it);
        const NodeDescriptor* descriptor = node == nullptr ? nullptr : graph.registry().find(node->typeId);
        if (descriptor == nullptr || descriptor->domain != graph::NodeDomain::Cpu) {
            continue;
        }
        bool isNeeded = !hasOutputs(*descriptor);  // Senke
        if (const auto found = consumers.find(*it); found != consumers.end()) {
            for (const NodeId consumer : found->second) {
                if (!isCpu(consumer) || needed.contains(consumer)) {
                    isNeeded = true;
                    break;
                }
            }
        }
        if (isNeeded) {
            needed.insert(*it);
        }
    }
    return needed;
}

KernelInputs collectInputs(const graph::Graph& graph, const NodeInstance& node,
                           const NodeDescriptor& descriptor, const ValueStore& store) {
    KernelInputs inputs;
    for (const graph::ParameterDescriptor& parameter : descriptor.parameters) {
        if (std::optional<graph::ParameterValue> value =
                graph::resolveParameter(graph, node, parameter, &store);
            value.has_value()) {
            inputs.set(parameter.key, std::move(*value));
        }
    }
    // Eingänge ohne gleichnamigen Parameter: nur ihr verbundener Wert, falls vorhanden.
    for (const graph::PortDescriptor& port : descriptor.ports) {
        if (port.direction != PortDirection::Input || descriptor.findParameter(port.key) != nullptr) {
            continue;
        }
        const graph::Connection* connection = graph.findInputConnection(node.id, port.key);
        if (connection == nullptr) {
            continue;
        }
        if (std::optional<graph::ParameterValue> value =
                store.value(connection->from.node, connection->from.portKey);
            value.has_value()) {
            inputs.set(port.key, std::move(*value));
        }
    }
    return inputs;
}

/// Prüft alle Ausgaben des Kernels gegen den Descriptor.
Result<void> validateOutputs(const NodeInstance& node, const NodeDescriptor& descriptor,
                             const KernelOutputs& outputs) {
    for (const auto& [key, value] : outputs.values()) {
        const graph::PortDescriptor* port = descriptor.findPort(key);
        if (port == nullptr || port->direction != PortDirection::Output) {
            return Error{ErrorCode::InvalidArgument,
                         describe(node) + ": Der Kernel liefert '" + key + "', einen Ausgang, den es nicht gibt"};
        }
        if (!graph::valueMatchesPortType(port->type, value)) {
            return Error{ErrorCode::InvalidArgument,
                         describe(node) + ": Der Wert für '" + key + "' hat nicht den Typ des Ausgangs"};
        }
    }
    for (const auto& [key, spectrum] : outputs.spectra()) {
        const graph::PortDescriptor* port = descriptor.findPort(key);
        if (port == nullptr || port->direction != PortDirection::Output || port->type != graph::PortType::AudioSpectrum) {
            return Error{ErrorCode::InvalidArgument,
                         describe(node) + ": Der Kernel liefert ein Spektrum für '" + key +
                             "', das kein Spektrum-Ausgang ist"};
        }
    }
    return {};
}

}  // namespace

void ValueEvaluator::dropStaleInstances(const graph::Graph& graph) {
    for (auto it = instances_.begin(); it != instances_.end();) {
        const NodeInstance* node = graph.findNode(it->first);
        if (node == nullptr || node->typeId != it->second.typeId) {
            it = instances_.erase(it);
        } else {
            ++it;
        }
    }
}

ValueEvaluator::Instance* ValueEvaluator::instanceFor(const NodeInstance& node) {
    const auto existing = instances_.find(node.id);
    if (existing != instances_.end()) {
        return &existing->second;
    }
    std::unique_ptr<ICpuKernel> kernel = kernels_.create(node.typeId);
    if (kernel == nullptr) {
        return nullptr;
    }
    const auto inserted = instances_.emplace(node.id, Instance{node.typeId, std::move(kernel)});
    return &inserted.first->second;
}

Result<void> ValueEvaluator::evaluate(const graph::Graph& graph, const FrameTime& time,
                                      ValueStore& store) {
    store.clear();

    Result<std::vector<NodeId>> order = graph::topologicalOrder(graph);
    if (!order.ok()) {
        return order.error();
    }
    dropStaleInstances(graph);
    const std::unordered_set<NodeId> needed = neededNodes(graph, order.value());

    for (const NodeId id : order.value()) {
        if (!needed.contains(id)) {
            continue;
        }
        const NodeInstance* node = graph.findNode(id);
        const NodeDescriptor* descriptor = graph.registry().find(node->typeId);

        Instance* instance = instanceFor(*node);
        if (instance == nullptr) {
            return Error{ErrorCode::NotFound, describe(*node) + ": Für den Typ ist kein CPU-Kernel registriert"};
        }

        const KernelInputs inputs = collectInputs(graph, *node, *descriptor, store);
        KernelOutputs outputs;
        instance->kernel->evaluate(KernelContext{id, time, inputs}, outputs);

        if (Result<void> valid = validateOutputs(*node, *descriptor, outputs); !valid.ok()) {
            return valid;
        }
        for (const auto& [key, value] : outputs.values()) {
            store.set(id, key, value);
        }
        for (const auto& [key, spectrum] : outputs.spectra()) {
            store.setSpectrum(id, key, spectrum);
        }
    }
    return {};
}

}  // namespace whitehole::values
