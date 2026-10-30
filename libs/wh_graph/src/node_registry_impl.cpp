#include "whitehole/graph/node_registry_impl.hpp"

#include <cctype>
#include <optional>
#include <set>
#include <utility>

#include "whitehole/graph/parameter_resolve.hpp"

namespace whitehole::graph {

namespace {

bool isIdentifier(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        const bool letterOrUnderscore = std::isalpha(c) != 0 || c == '_';
        const bool digit = std::isdigit(c) != 0;
        if (!(letterOrUnderscore || (i > 0 && digit))) {
            return false;
        }
    }
    return true;
}

Error invalid(const std::string& typeId, const std::string& detail) {
    return Error{ErrorCode::InvalidArgument, "Node-Typ '" + typeId + "': " + detail};
}

/// Liefert eine Fehlermeldung oder einen leeren Text, wenn alles stimmt.
std::string validate(const NodeDescriptor& descriptor) {
    std::set<std::string_view> portKeys;
    for (const PortDescriptor& port : descriptor.ports) {
        if (!isIdentifier(port.key)) {
            return "Port-Schlüssel '" + port.key + "' ist kein gültiger Bezeichner";
        }
        if (!portKeys.insert(port.key).second) {
            return "Port-Schlüssel '" + port.key + "' kommt doppelt vor";
        }
    }

    std::set<std::string_view> parameterKeys;
    for (const ParameterDescriptor& parameter : descriptor.parameters) {
        if (!isIdentifier(parameter.key)) {
            return "Parameter-Schlüssel '" + parameter.key + "' ist kein gültiger Bezeichner";
        }
        if (!parameterKeys.insert(parameter.key).second) {
            return "Parameter-Schlüssel '" + parameter.key + "' kommt doppelt vor";
        }
        if (parameter.minValue.has_value() && parameter.maxValue.has_value() &&
            *parameter.minValue > *parameter.maxValue) {
            return "Parameter '" + parameter.key + "' hat minValue > maxValue";
        }
    }

    // Modulation: Ein Eingang mit dem Schlüssel eines Parameters muss zu dessen Typ passen.
    for (const ParameterDescriptor& parameter : descriptor.parameters) {
        const PortDescriptor* port = descriptor.findPort(parameter.key);
        if (port == nullptr || port->direction != PortDirection::Input) {
            continue;
        }
        const std::optional<PortType> expected = modulationPortType(parameter.defaultValue);
        if (!expected.has_value() || *expected != port->type) {
            return "Eingang '" + parameter.key +
                   "' teilt den Schlüssel mit einem Parameter (Modulation), hat aber nicht den "
                   "dazu passenden Typ";
        }
    }
    return {};
}

}  // namespace

const NodeDescriptor* NodeRegistry::find(std::string_view typeId) const {
    const auto it = descriptors_.find(typeId);
    return it == descriptors_.end() ? nullptr : &it->second;
}

std::vector<const NodeDescriptor*> NodeRegistry::all() const {
    std::vector<const NodeDescriptor*> result;
    result.reserve(descriptors_.size());
    for (const auto& entry : descriptors_) {
        result.push_back(&entry.second);
    }
    return result;
}

Result<void> NodeRegistry::registerNode(NodeDescriptor descriptor) {
    if (descriptor.typeId.empty()) {
        return Error{ErrorCode::InvalidArgument, "Node-Typ ohne Typ-ID"};
    }
    if (descriptors_.contains(descriptor.typeId)) {
        return Error{ErrorCode::AlreadyExists,
                     "Node-Typ '" + descriptor.typeId + "' ist bereits registriert"};
    }
    if (const std::string problem = validate(descriptor); !problem.empty()) {
        return invalid(descriptor.typeId, problem);
    }

    std::string key = descriptor.typeId;
    descriptors_.emplace(std::move(key), std::move(descriptor));
    return {};
}

}  // namespace whitehole::graph
