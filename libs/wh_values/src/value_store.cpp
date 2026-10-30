#include "whitehole/values/value_store.hpp"

#include <utility>

namespace whitehole::values {

void ValueStore::set(NodeId node, std::string_view portKey, graph::ParameterValue value) {
    auto& ports = values_[node];
    const auto it = ports.find(portKey);
    if (it != ports.end()) {
        it->second = std::move(value);
        return;
    }
    ports.emplace(std::string{portKey}, std::move(value));
    ++count_;
}

std::optional<graph::ParameterValue> ValueStore::value(NodeId node, std::string_view portKey) const {
    const auto nodeIt = values_.find(node);
    if (nodeIt == values_.end()) {
        return std::nullopt;
    }
    const auto portIt = nodeIt->second.find(portKey);
    if (portIt == nodeIt->second.end()) {
        return std::nullopt;
    }
    return portIt->second;
}

void ValueStore::clear() noexcept {
    values_.clear();
    count_ = 0;
}

}  // namespace whitehole::values
