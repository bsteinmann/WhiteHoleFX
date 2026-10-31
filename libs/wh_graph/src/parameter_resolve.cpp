// parameter_resolve.cpp: Umsetzung zu parameter_resolve.hpp. Modulation von Parametern durch verbundene Werte.
#include "whitehole/graph/parameter_resolve.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

namespace whitehole::graph {

namespace {

double clampTo(double value, const ParameterDescriptor& parameter) {
    if (parameter.minValue.has_value()) {
        value = std::max(value, *parameter.minValue);
    }
    if (parameter.maxValue.has_value()) {
        value = std::min(value, *parameter.maxValue);
    }
    return value;
}

bool isFinite(const ColorRgba& color) {
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) &&
           std::isfinite(color.a);
}

}  // namespace

std::optional<PortType> modulationPortType(const ParameterValue& value) noexcept {
    if (std::holds_alternative<double>(value)) {
        return PortType::Scalar;
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return PortType::Integer;
    }
    if (std::holds_alternative<bool>(value)) {
        return PortType::Boolean;
    }
    if (std::holds_alternative<ColorRgba>(value)) {
        return PortType::Color;
    }
    return std::nullopt;
}

bool valueMatchesPortType(PortType type, const ParameterValue& value) noexcept {
    switch (type) {
        case PortType::Scalar:
            return std::holds_alternative<double>(value);
        case PortType::Integer:
            return std::holds_alternative<std::int64_t>(value);
        case PortType::Boolean:
        case PortType::Trigger:
            return std::holds_alternative<bool>(value);
        case PortType::Color:
            return std::holds_alternative<ColorRgba>(value);
        case PortType::Text:
            return std::holds_alternative<std::string>(value);
        default:
            return false;
    }
}

void addModulationPorts(NodeDescriptor& descriptor) {
    for (const ParameterDescriptor& parameter : descriptor.parameters) {
        const std::optional<PortType> type = modulationPortType(parameter.defaultValue);
        if (!type.has_value() || descriptor.findPort(parameter.key) != nullptr) {
            continue;
        }
        descriptor.ports.push_back(PortDescriptor{parameter.key, parameter.displayName, *type,
                                                  PortDirection::Input, false});
    }
}

std::optional<ParameterValue> convertForParameter(const ParameterValue& source,
                                                  const ParameterDescriptor& target) {
    if (std::holds_alternative<double>(target.defaultValue)) {
        double number = 0.0;
        if (const auto* real = std::get_if<double>(&source)) {
            number = *real;
        } else if (const auto* integer = std::get_if<std::int64_t>(&source)) {
            number = static_cast<double>(*integer);
        } else if (const auto* flag = std::get_if<bool>(&source)) {
            number = *flag ? 1.0 : 0.0;
        } else {
            return std::nullopt;
        }
        if (!std::isfinite(number)) {
            return std::nullopt;
        }
        return ParameterValue{clampTo(number, target)};
    }

    if (std::holds_alternative<std::int64_t>(target.defaultValue)) {
        const auto* integer = std::get_if<std::int64_t>(&source);
        if (integer == nullptr) {
            return std::nullopt;
        }
        if (!target.minValue.has_value() && !target.maxValue.has_value()) {
            return source;  // keine Grenzen: Wert unverändert, kein Umweg über double
        }
        return ParameterValue{
            static_cast<std::int64_t>(clampTo(static_cast<double>(*integer), target))};
    }

    if (std::holds_alternative<bool>(target.defaultValue)) {
        return std::holds_alternative<bool>(source) ? std::optional<ParameterValue>{source}
                                                    : std::nullopt;
    }

    if (std::holds_alternative<ColorRgba>(target.defaultValue)) {
        const auto* color = std::get_if<ColorRgba>(&source);
        if (color == nullptr || !isFinite(*color)) {
            return std::nullopt;
        }
        return source;
    }
    return std::nullopt;
}

std::optional<ParameterValue> resolveParameter(const Graph& graph, const NodeInstance& node,
                                               const ParameterDescriptor& parameter,
                                               const IValueLookup* values) {
    if (values != nullptr) {
        const Connection* connection = graph.findInputConnection(node.id, parameter.key);
        if (connection != nullptr) {
            const std::optional<ParameterValue> delivered =
                values->value(connection->from.node, connection->from.portKey);
            if (delivered.has_value()) {
                if (std::optional<ParameterValue> converted = convertForParameter(*delivered, parameter);
                    converted.has_value()) {
                    return converted;
                }
            }
        }
    }

    const auto stored = node.parameters.find(parameter.key);
    if (stored == node.parameters.end()) {
        return std::nullopt;
    }
    return stored->second;
}

}  // namespace whitehole::graph
