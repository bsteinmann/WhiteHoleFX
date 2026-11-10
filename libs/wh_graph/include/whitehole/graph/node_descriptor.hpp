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
    /// Issue #25: Ein Eingang von Nodes mit mehreren Eingängen (Blend, Mix ...). Im nachsichtigen Modus (Standard) gilt er
    /// unverbunden als durchsichtiges Schwarz; nur im strengen Modus (Graph::strictInputs) ist er ein Pflicht-Eingang.
    bool optionalWhenLenient{false};
};

/// Ob ein unverbundener Eingang den Graphen unvollständig macht (Fehler beim Kompilieren, LEDs aus).
[[nodiscard]] inline bool isRequiredInput(const PortDescriptor& port, bool strictInputs) noexcept {
    return port.required && (strictInputs || !port.optionalWhenLenient);
}

struct ParameterDescriptor {
    std::string key;
    std::string displayName;
    ParameterValue defaultValue;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    /// Bei einer Ganzzahl: Namen der Auswahlmöglichkeiten, der Wert ist der Index (0 = erster Name).
    /// Die Oberfläche zeigt dann ein Auswahlfeld statt einer Zahl. Leer = gewöhnliche Zahl.
    std::vector<std::string> choices{};
};

/// Ob der Node im Editor einen Anzeigestreifen bekommt (Platz unter den Ports), den sein Kernel füllt (KernelOutputs::setMonitor).
enum class LiveView { None, Spectrum, Wave };

struct NodeDescriptor {
    std::string typeId;  // eindeutig, z. B. "whitehole.noise.simplex"
    std::string displayName;
    std::string category;
    NodeDomain domain{NodeDomain::Field};
    std::vector<PortDescriptor> ports;
    std::vector<ParameterDescriptor> parameters;
    LiveView liveView{LiveView::None};

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
