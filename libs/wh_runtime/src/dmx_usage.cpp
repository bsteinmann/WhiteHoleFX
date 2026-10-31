#include "whitehole/runtime/dmx_usage.hpp"

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

}  // namespace

DmxUsage findDmxUsage(const graph::Graph& graph) {
    DmxUsage usage;
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (node.typeId != "wh.value.dmx" && node.typeId != "wh.value.dmxcolor") {
            continue;
        }
        const auto universe = static_cast<std::uint32_t>(std::clamp<std::int64_t>(integerParameter(node, "universe", 1), 0, 63999));
        (integerParameter(node, "source", 0) == 1 ? usage.sacn : usage.artnet).insert(universe);
    }
    return usage;
}

}  // namespace whitehole::runtime
