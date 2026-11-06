// connection_scale.cpp: Umsetzung zu connection_scale.hpp.
#include "whitehole/runtime/connection_scale.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <variant>

namespace whitehole::runtime {

std::optional<std::pair<double, double>> suggestConnectionScale(const ParameterInfo& parameter) {
    const bool isNumber = std::holds_alternative<double>(parameter.value) || std::holds_alternative<std::int64_t>(parameter.value);
    if (!isNumber || !parameter.choices.empty()) {
        return std::nullopt;
    }
    if (parameter.minValue && parameter.maxValue && *parameter.maxValue > *parameter.minValue) {
        return std::pair{*parameter.maxValue - *parameter.minValue, *parameter.minValue};
    }
    std::string key = parameter.key;
    std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (key.find("angle") != std::string::npos || key.find("rotation") != std::string::npos) {
        return std::pair{360.0, 0.0};
    }
    return std::nullopt;
}

}  // namespace whitehole::runtime
