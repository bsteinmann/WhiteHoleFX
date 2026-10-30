// fake_value_lookup.hpp: IValueLookup mit frei setzbaren Werten für Tests.
// Damit prüfen Compiler- und Graph-Tests die Modulation, ohne wh_values zu brauchen.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "whitehole/graph/value_lookup.hpp"

namespace whitehole::test {

class FakeValueLookup final : public graph::IValueLookup {
public:
    void set(NodeId node, const std::string& portKey, graph::ParameterValue value) {
        values_.insert_or_assign({static_cast<std::uint64_t>(node.value()), portKey}, std::move(value));
    }

    void clear() { values_.clear(); }

    [[nodiscard]] std::optional<graph::ParameterValue> value(NodeId node,
                                                             std::string_view portKey) const override {
        const auto it = values_.find({static_cast<std::uint64_t>(node.value()), std::string{portKey}});
        if (it == values_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    std::map<std::pair<std::uint64_t, std::string>, graph::ParameterValue> values_;
};

}  // namespace whitehole::test
