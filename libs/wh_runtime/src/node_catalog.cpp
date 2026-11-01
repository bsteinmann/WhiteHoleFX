// node_catalog.cpp: Text für --list-nodes und --describe-node.
#include "whitehole/runtime/node_catalog.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <tuple>
#include <variant>
#include <vector>

namespace whitehole::runtime {

namespace {

std::string number(double value) {
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%g", value);
    return buffer;
}

/// Der Wert so, wie ihn `set <parameter> …` in einer Projektdatei braucht.
std::string valueText(const graph::ParameterValue& value) {
    struct Visitor {
        std::string operator()(bool v) const { return v ? "true" : "false"; }
        std::string operator()(std::int64_t v) const { return std::to_string(v); }
        std::string operator()(double v) const { return number(v); }
        std::string operator()(const Vec2& v) const { return number(v.x) + " " + number(v.y); }
        std::string operator()(const Vec3& v) const { return number(v.x) + " " + number(v.y) + " " + number(v.z); }
        std::string operator()(const ColorRgba& v) const {
            return number(v.r) + " " + number(v.g) + " " + number(v.b) + " " + number(v.a);
        }
        std::string operator()(const std::string& v) const { return "\"" + v + "\""; }
    };
    return std::visit(Visitor{}, value);
}

const char* kindName(const graph::ParameterValue& value) {
    switch (value.index()) {
        case 0: return "Ja/Nein (true/false)";
        case 1: return "Ganzzahl";
        case 2: return "Zahl";
        case 3: return "2 Zahlen";
        case 4: return "3 Zahlen";
        case 5: return "Farbe (r g b a)";
        default: return "Text";
    }
}

}  // namespace

const char* portTypeName(graph::PortType type) noexcept {
    switch (type) {
        case graph::PortType::Image: return "Image";
        case graph::PortType::Field: return "Field";
        case graph::PortType::Scalar: return "Scalar";
        case graph::PortType::Integer: return "Integer";
        case graph::PortType::Boolean: return "Boolean";
        case graph::PortType::Color: return "Color";
        case graph::PortType::Palette: return "Palette";
        case graph::PortType::Text: return "Text";
        case graph::PortType::Trigger: return "Trigger";
        case graph::PortType::AudioSpectrum: return "AudioSpectrum";
        case graph::PortType::PointCloud: return "PointCloud";
        case graph::PortType::AudioWave: return "AudioWave";
    }
    return "?";
}

std::string listNodes(const graph::INodeRegistry& registry) {
    std::vector<const graph::NodeDescriptor*> types = registry.all();
    std::sort(types.begin(), types.end(), [](const auto* a, const auto* b) {
        return std::tie(a->category, a->displayName, a->typeId) < std::tie(b->category, b->displayName, b->typeId);
    });
    std::ostringstream out;
    for (const graph::NodeDescriptor* type : types) {
        out << type->typeId << "  " << (type->category.empty() ? "-" : type->category) << "  " << type->displayName << "\n";
    }
    return out.str();
}

Result<std::string> describeNode(const graph::INodeRegistry& registry, std::string_view typeId) {
    const graph::NodeDescriptor* type = registry.find(std::string{typeId});
    if (type == nullptr) {
        std::string hint;
        for (const graph::NodeDescriptor* candidate : registry.all()) {
            if (candidate->typeId.find(typeId) != std::string::npos && !typeId.empty()) {
                hint += (hint.empty() ? "" : ", ") + candidate->typeId;
            }
        }
        return Error{ErrorCode::NotFound, "Unbekannter Node-Typ \"" + std::string{typeId} + "\"" +
                                              (hint.empty() ? " (whitehole_cli --list-nodes zeigt alle)" : "; meinst du: " + hint)};
    }
    std::ostringstream out;
    out << type->typeId << "  " << type->displayName << "  (" << (type->category.empty() ? "-" : type->category) << ")\n";
    out << "\nPorts (in der Datei: connect <node>.<port> -> <node>.<port>):\n";
    for (const graph::PortDescriptor& port : type->ports) {
        const bool modulation = type->findParameter(port.key) != nullptr;
        out << "  " << (port.direction == graph::PortDirection::Input ? "Eingang " : "Ausgang ") << port.key << "  "
            << port.displayName << "  " << portTypeName(port.type);
        if (port.required) {
            out << "  (Pflicht)";
        }
        if (modulation) {
            out << "  (steuert den gleichnamigen Parameter)";
        }
        out << "\n";
    }
    out << "\nParameter (in der Datei: set <parameter> <wert>):\n";
    if (type->parameters.empty()) {
        out << "  (keine)\n";
    }
    for (const graph::ParameterDescriptor& parameter : type->parameters) {
        out << "  " << parameter.key << "  " << parameter.displayName << "  " << kindName(parameter.defaultValue)
            << "  Standard " << valueText(parameter.defaultValue);
        if (parameter.minValue || parameter.maxValue) {
            out << "  Bereich " << (parameter.minValue ? number(*parameter.minValue) : std::string{"..."}) << " bis "
                << (parameter.maxValue ? number(*parameter.maxValue) : std::string{"..."});
        }
        if (!parameter.choices.empty()) {
            out << "  Auswahl:";
            for (std::size_t i = 0; i < parameter.choices.size(); ++i) {
                out << " " << i << "=" << parameter.choices[i];
            }
        }
        out << "\n";
    }
    return out.str();
}

}  // namespace whitehole::runtime
