// look_thumbnail.cpp: Mittelwert-Verkleinerung für das Look-Standbild (siehe look_thumbnail.hpp).
#include "whitehole/runtime/look_thumbnail.hpp"

#include <algorithm>
#include <cstddef>

namespace whitehole::runtime {

std::optional<project::LookThumbnail> makeLookThumbnail(const preview::Image& image, std::uint32_t width,
                                                        std::uint32_t height) {
    if (width == 0 || height == 0 || width > project::kMaxThumbnailSide || height > project::kMaxThumbnailSide ||
        image.width < width || image.height < height ||
        image.rgba.size() < static_cast<std::size_t>(image.width) * image.height * 4) {
        return std::nullopt;
    }
    project::LookThumbnail out;
    out.width = width;
    out.height = height;
    out.rgb.resize(static_cast<std::size_t>(width) * height * 3);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t y0 = static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * image.height / height);
        const std::uint32_t y1 = std::max(y0 + 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(y + 1) * image.height / height));
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t x0 = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * image.width / width);
            const std::uint32_t x1 = std::max(x0 + 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(x + 1) * image.width / width));
            std::uint64_t sum[3] = {0, 0, 0};
            for (std::uint32_t py = y0; py < y1; ++py) {
                for (std::uint32_t px = x0; px < x1; ++px) {
                    const std::size_t at = (static_cast<std::size_t>(py) * image.width + px) * 4;
                    const std::uint32_t alpha = image.rgba[at + 3];
                    for (std::size_t c = 0; c < 3; ++c) {
                        sum[c] += static_cast<std::uint64_t>(image.rgba[at + c]) * alpha / 255;
                    }
                }
            }
            const std::uint64_t count = static_cast<std::uint64_t>(y1 - y0) * (x1 - x0);
            const std::size_t o = (static_cast<std::size_t>(y) * width + x) * 3;
            for (std::size_t c = 0; c < 3; ++c) {
                out.rgb[o + c] = static_cast<std::uint8_t>(sum[c] / count);
            }
        }
    }
    return out;
}

}  // namespace whitehole::runtime
