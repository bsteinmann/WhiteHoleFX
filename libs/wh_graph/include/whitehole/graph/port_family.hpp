// port_family.hpp: ordnet Porttypen groben Familien zu und beantwortet "passt dieser Node an diesen Port?".
//
// Warum es das gibt: Das Palettenfenster des Studios soll Nodes nach dem markieren, was sie liefern (farbiger
// Punkt), und beim Ziehen eines Kabels nur die Nodes hervorheben, die sich anschließen lassen. Zwölf Porttypen
// ergäben einen Regenbogen; fünf Familien reichen zum Unterscheiden. Die Regeln stehen hier und nicht in der
// Oberfläche, damit sie ohne Fenster geprüft werden (tests/graph/port_family_test.cpp).
#pragma once

#include <optional>

#include "whitehole/graph/node_descriptor.hpp"
#include "whitehole/graph/port_type.hpp"

namespace whitehole::graph {

enum class PortFamily {
    Image,  // Field, Image: das Bild, das auf die LEDs kommt
    Value,  // Scalar, Integer, Boolean, Trigger: Steuerwerte
    Color,  // Color, Palette
    Audio,  // Spektrum, Wellenform
    Other   // Text, Punktwolke
};

[[nodiscard]] constexpr PortFamily portFamily(PortType type) noexcept {
    switch (type) {
        case PortType::Field:
        case PortType::Image:
            return PortFamily::Image;
        case PortType::Scalar:
        case PortType::Integer:
        case PortType::Boolean:
        case PortType::Trigger:
            return PortFamily::Value;
        case PortType::Color:
        case PortType::Palette:
            return PortFamily::Color;
        case PortType::AudioSpectrum:
        case PortType::AudioWave:
            return PortFamily::Audio;
        case PortType::Text:
        case PortType::PointCloud:
            return PortFamily::Other;
    }
    return PortFamily::Other;
}

/// Die Familie des ersten Ausgangs eines Nodes; ohne Ausgang nichts.
[[nodiscard]] inline std::optional<PortFamily> outputFamily(const NodeDescriptor& node) noexcept {
    for (const PortDescriptor& port : node.ports) {
        if (port.direction == PortDirection::Output) {
            return portFamily(port.type);
        }
    }
    return std::nullopt;
}

/// Hat der Node einen Eingang (auch einen Steuer-Eingang eines Parameters), an den ein Ausgang vom Typ `from` passt?
[[nodiscard]] inline bool acceptsInputFrom(const NodeDescriptor& node, PortType from) noexcept {
    for (const PortDescriptor& port : node.ports) {
        if (port.direction == PortDirection::Input && isConnectable(from, port.type)) {
            return true;
        }
    }
    return false;
}

/// Hat der Node einen Ausgang, der sich an einen Eingang vom Typ `to` anschließen lässt?
[[nodiscard]] inline bool offersOutputFor(const NodeDescriptor& node, PortType to) noexcept {
    for (const PortDescriptor& port : node.ports) {
        if (port.direction == PortDirection::Output && isConnectable(port.type, to)) {
            return true;
        }
    }
    return false;
}

}  // namespace whitehole::graph
