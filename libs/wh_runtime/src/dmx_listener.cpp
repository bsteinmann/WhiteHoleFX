// dmx_listener.cpp: Umsetzung zu dmx_listener.hpp.
#include "whitehole/runtime/dmx_listener.hpp"

#include <algorithm>
#include <array>
#include <variant>

#include "whitehole/runtime/node_display_name.hpp"

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
            return DmxInputListener{DmxInputListener::Kind::Node, nodeDisplayName(graph, node.id), nodeChannel, count};
        }
    }
    return std::nullopt;
}

std::vector<DmxValueNodeRef> findDmxValueNodes(const graph::Graph& graph) {
    std::vector<DmxValueNodeRef> result;
    const auto names = nodeDisplayNames(graph);
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (node.typeId == "wh.value.dmx" || node.typeId == "wh.value.dmxcolor") {
            const auto it = names.find(node.id.value());
            result.push_back(DmxValueNodeRef{node.id, it != names.end() ? it->second : std::string{}, std::nullopt});
        }
    }
    return result;
}

namespace {

/// Wie viele echte DMX-Bytes der Node je Frame schreibt (= wie viele Pool-Kanäle ab seinem Parameter
/// "channel" er belegt) - dieselbe Rechnung wie die Kernel selbst (builtin_dmx_out.cpp), hier nur für die
/// Anzeige/Patch-Erzeugung gebraucht, nicht fürs eigentliche Schreiben.
std::uint16_t outputNodeCount(const graph::NodeInstance& node) {
    if (node.typeId == "wh.output.dmx") {
        const std::int64_t channels = std::clamp<std::int64_t>(integerParameter(node, "count", 8), 1, 8);
        const bool wide = boolParameter(node, "bits16", false);
        return static_cast<std::uint16_t>(channels * (wide ? 2 : 1));
    }
    if (node.typeId == "wh.output.dmxcolor") {
        // 0 RGB (3), 1 RGBW (4), 2 Dimmer+RGB (4), 3 Dimmer+RGBW (5) - siehe DmxColorOutKernel.
        static constexpr std::array<std::uint16_t, 4> kCounts{3, 4, 4, 5};
        const std::size_t mode = static_cast<std::size_t>(std::clamp<std::int64_t>(integerParameter(node, "mode", 0), 0, 3));
        return kCounts[mode];
    }
    return 1;
}

}  // namespace

std::vector<DmxOutputValueNodeRef> findDmxOutputValueNodes(const graph::Graph& graph) {
    std::vector<DmxOutputValueNodeRef> result;
    const auto names = nodeDisplayNames(graph);
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (node.typeId == "wh.output.dmx" || node.typeId == "wh.output.dmxcolor") {
            DmxOutputValueNodeRef ref;
            ref.id = node.id;
            const auto it = names.find(node.id.value());
            ref.label = it != names.end() ? it->second : std::string{};
            ref.poolChannel = static_cast<std::uint16_t>(std::clamp<std::int64_t>(integerParameter(node, "channel", 1), 1, 512));
            ref.count = outputNodeCount(node);
            result.push_back(ref);
        }
    }
    return result;
}

}  // namespace whitehole::runtime
