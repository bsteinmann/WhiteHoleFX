// port_type.hpp: Datentypen der Node-Ports und Regeln, was sich verbinden lässt.
#pragma once

#include <cstdint>

namespace whitehole::graph {

enum class PortType : std::uint8_t {
    Image,          // Raster-Domäne: Textur
    Field,          // Field-Domäne: Funktion über Koordinate und Zeit
    Scalar,         // Gleitkommazahl
    Integer,
    Boolean,
    Color,
    Palette,
    Text,
    Trigger,        // Einzelimpuls (z. B. Beat)
    AudioSpectrum,
    PointCloud,
    AudioWave,      // 32 Werte der Wellenform, -1..1 (Transport wie AudioSpectrum)
};

/// Porttypen, die 32 Zahlen als eigenen Kanal tragen (Spektrum und Wellenform): KernelOutputs::setSpectrum,
/// IValueLookup::spectrum und im Shader `@spectrum(port, i)`. Sie sind untereinander nicht verbindbar.
[[nodiscard]] constexpr bool carriesBands(PortType type) noexcept {
    return type == PortType::AudioSpectrum || type == PortType::AudioWave;
}

/// Darf ein Ausgang vom Typ `from` mit einem Eingang vom Typ `to` verbunden
/// werden? Gleiche Typen immer; dazu wenige verlustfreie Überführungen:
///   Field <-> Image        (Rasterisieren bzw. als Textur abtasten)
///   Scalar, Color -> Field (konstantes Feld)
///   Integer, Boolean -> Scalar
///   Trigger -> Boolean
[[nodiscard]] constexpr bool isConnectable(PortType from, PortType to) noexcept {
    if (from == to) {
        return true;
    }
    switch (from) {
        case PortType::Field:
            return to == PortType::Image;
        case PortType::Image:
            return to == PortType::Field;
        case PortType::Scalar:
        case PortType::Color:
            return to == PortType::Field;
        case PortType::Integer:
        case PortType::Boolean:
            return to == PortType::Scalar;
        case PortType::Trigger:
            return to == PortType::Boolean;
        default:
            return false;
    }
}

}  // namespace whitehole::graph
