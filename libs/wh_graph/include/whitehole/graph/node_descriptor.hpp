// node_descriptor.hpp: Beschreibung eines Node-Typs (Ports, Parameter, Domäne).
// Der Graph kennt Nodes nur über diese Beschreibung, nie über konkrete Klassen.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "whitehole/core/types.hpp"
#include "whitehole/graph/port_type.hpp"

namespace whitehole::graph {

/// Wo ein Node ausgeführt wird.
enum class NodeDomain : std::uint8_t {
    Field,   // Shader-Funktion über Koordinate und Zeit, wird mit anderen verschmolzen
    Raster,  // arbeitet auf Texturen (Blur, Feedback, Video)
    Cpu,     // CPU-Node (z. B. Python, Audioanalyse), Ergebnis wird hochgeladen
};

enum class PortDirection : std::uint8_t { Input, Output };

using ParameterValue =
    std::variant<bool, std::int64_t, double, Vec2, Vec3, ColorRgba, std::string>;

struct PortDescriptor {
    std::string key;  // stabiler Schlüssel für Verbindungen und Dateien
    std::string displayName;
    PortType type{PortType::Image};
    PortDirection direction{PortDirection::Input};
    bool required{false};
};

struct ParameterDescriptor {
    std::string key;
    std::string displayName;
    ParameterValue defaultValue;
    std::optional<double> minValue;
    std::optional<double> maxValue;
};

struct NodeDescriptor {
    std::string typeId;  // eindeutig, z. B. "whitehole.noise.simplex"
    std::string displayName;
    std::string category;
    NodeDomain domain{NodeDomain::Field};
    std::vector<PortDescriptor> ports;
    std::vector<ParameterDescriptor> parameters;

    [[nodiscard]] const PortDescriptor* findPort(std::string_view key) const noexcept {
        for (const auto& port : ports) {
            if (port.key == key) {
                return &port;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const ParameterDescriptor* findParameter(std::string_view key) const noexcept {
        for (const auto& parameter : parameters) {
            if (parameter.key == key) {
                return &parameter;
            }
        }
        return nullptr;
    }
};

}  // namespace whitehole::graph
