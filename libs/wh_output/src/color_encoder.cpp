#include "whitehole/output/color_encoder.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace whitehole::output {

namespace {

constexpr std::size_t kTableSize = 65536;

/// Ordnet (r, g, b) in die Reihenfolge der LED: Eintrag i sagt, welcher Eingangskanal
/// (0 = r, 1 = g, 2 = b) an Byte i geschrieben wird.
constexpr std::array<std::uint8_t, 3> sourceChannels(ColorOrder order) noexcept {
    switch (order) {
        case ColorOrder::RGB: return {0, 1, 2};
        case ColorOrder::RBG: return {0, 2, 1};
        case ColorOrder::GRB: return {1, 0, 2};
        case ColorOrder::GBR: return {1, 2, 0};
        case ColorOrder::BRG: return {2, 0, 1};
        case ColorOrder::BGR: return {2, 1, 0};
    }
    return {0, 1, 2};
}

}  // namespace

Result<ColorEncoder> ColorEncoder::create(const ColorSettings& settings) {
    if (!std::isfinite(settings.gamma) || settings.gamma < 0.1F || settings.gamma > 10.0F) {
        return Error{ErrorCode::InvalidArgument, "Gamma muss zwischen 0,1 und 10 liegen"};
    }
    if (!std::isfinite(settings.brightness) || settings.brightness < 0.0F || settings.brightness > 1.0F) {
        return Error{ErrorCode::InvalidArgument, "Die Helligkeit muss zwischen 0 und 1 liegen"};
    }

    std::vector<std::uint8_t> table(kTableSize);
    const double exponent = static_cast<double>(settings.gamma);
    for (std::size_t i = 0; i < kTableSize; ++i) {
        const double linear = static_cast<double>(i) / static_cast<double>(kTableSize - 1);
        const double encoded = std::pow(linear, exponent);
        table[i] = static_cast<std::uint8_t>(std::lround(std::clamp(encoded, 0.0, 1.0) * 255.0));
    }
    return ColorEncoder{settings, std::move(table)};
}

std::uint8_t ColorEncoder::channel(float linear) const noexcept {
    if (!(linear > 0.0F)) {  // fängt auch NaN
        return 0;
    }
    const float scaled = std::min(linear, 1.0F) * static_cast<float>(kTableSize - 1);
    return table_[static_cast<std::size_t>(std::lround(scaled))];
}

std::array<std::uint8_t, 3> ColorEncoder::encode(const ColorRgba& color) const noexcept {
    const float alpha = std::isfinite(color.a) ? std::clamp(color.a, 0.0F, 1.0F) : 0.0F;
    const float scale = alpha * settings_.brightness;
    const std::array<float, 3> rgb{color.r * scale, color.g * scale, color.b * scale};
    const std::array<std::uint8_t, 3> source = sourceChannels(settings_.order);
    return {channel(rgb[source[0]]), channel(rgb[source[1]]), channel(rgb[source[2]])};
}

}  // namespace whitehole::output
