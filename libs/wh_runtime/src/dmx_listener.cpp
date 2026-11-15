// dmx_listener.cpp: Umsetzung zu dmx_listener.hpp.
#include "whitehole/runtime/dmx_listener.hpp"

#include <algorithm>
#include <variant>

namespace whitehole::runtime {

namespace {

std::int64_t integerParameter(const graph::NodeInstance& node, const char* key, std::int64_t fallback) {
    const auto it = node.parameters.find(key);
    if (it == node.parameters.end()) {
        return fallback;
    }
    if (const auto* value = std::get_if<std::int64_t>(&it->second)) {
        return *value;
    }
    return fallback;
}

bool boolParameter(const graph::NodeInstance& node, const char* key, bool fallback) {
    const auto it = node.parameters.find(key);
    if (it == node.parameters.end()) {
        return fallback;
    }
    if (const auto* value = std::get_if<bool>(&it->second)) {
        return *value;
    }
    return fallback;
}

}  // namespace

std::optional<DmxInputListener> findDmxListenerInGraph(const graph::Graph& graph, input::DmxProtocol protocol,
                                                        std::uint32_t universe, std::uint32_t channel) {
    for (const graph::NodeInstance& node : graph.nodes()) {
        const bool isChannel = node.typeId == "wh.value.dmx";
        const bool isColor = node.typeId == "wh.value.dmxcolor";
        if (!isChannel && !isColor) {
            continue;
        }
        const auto nodeProtocol = integerParameter(node, "source", 0) == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
        if (nodeProtocol != protocol) {
            continue;
        }
        const auto nodeUniverse = static_cast<std::uint32_t>(std::clamp<std::int64_t>(integerParameter(node, "universe", 1), 0, 63999));
        if (nodeUniverse != universe) {
            continue;
        }
        const auto nodeChannel = static_cast<std::uint32_t>(std::clamp<std::int64_t>(integerParameter(node, "channel", 1), 1, 512));
        const std::uint32_t count = isChannel ? (boolParameter(node, "bits16", false) ? 2U : 1U)
                                               : (integerParameter(node, "mode", 0) == 1 ? 4U : 3U);
        if (channel >= nodeChannel && channel < nodeChannel + count) {
            return DmxInputListener{node.label, nodeChannel, count};
        }
    }
    return std::nullopt;
}

std::vector<DmxValueNodeRef> findDmxValueNodes(const graph::Graph& graph) {
    std::vector<DmxValueNodeRef> result;
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (node.typeId == "wh.value.dmx" || node.typeId == "wh.value.dmxcolor") {
            result.push_back(DmxValueNodeRef{node.id, node.label});
        }
    }
    return result;
}

}  // namespace whitehole::runtime
