// types.hpp: kleine, Qt-freie Grundtypen für alle Module.
#pragma once

#include <cstdint>

namespace whitehole {

struct Vec2 {
    float x{0.0F};
    float y{0.0F};

    bool operator==(const Vec2&) const = default;
};

struct Vec3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    bool operator==(const Vec3&) const = default;
};

/// Farbe als lineare Float-Werte (0..1). Keine Gammakorrektur im Typ selbst;
/// Gamma und Farbreihenfolge wendet erst der Ausgabe-Pass an.
struct ColorRgba {
    float r{0.0F};
    float g{0.0F};
    float b{0.0F};
    float a{1.0F};

    bool operator==(const ColorRgba&) const = default;
};

/// Größe eines Canvas oder einer Textur. Tiefe 1 bedeutet 2D, Tiefe > 1 ein
/// 3D-Volumen (z. B. für LED-Cubes).
struct Extent3 {
    std::uint32_t width{1};
    std::uint32_t height{1};
    std::uint32_t depth{1};

    [[nodiscard]] constexpr bool is3D() const noexcept { return depth > 1; }

    [[nodiscard]] constexpr std::uint64_t texelCount() const noexcept {
        return std::uint64_t{width} * height * depth;
    }

    bool operator==(const Extent3&) const = default;
};

}  // namespace whitehole
