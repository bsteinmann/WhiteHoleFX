// grid.cpp: Umsetzung zu grid.hpp.
#include "whitehole/preview/grid.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::preview {

float snapToGrid(float value, float step) noexcept {
    if (!(step > 0.0F) || !std::isfinite(value)) {
        return value;
    }
    return std::round(value / step) * step;
}

float effectiveGridStep(float step, float pixelsPerUnit) noexcept {
    if (!(step > 0.0F) || !(pixelsPerUnit > 0.0F)) {
        return 0.0F;
    }
    float result = step;
    // Reihenfolge 1, 2, 5, 10 ... der Zehnerpotenzen
    constexpr float kFactors[] = {2.0F, 2.5F, 2.0F};
    int index = 0;
    while (result * pixelsPerUnit < kMinGridPixels && result < 1e9F) {
        result *= kFactors[index % 3];
        ++index;
    }
    return result;
}

namespace {

void addLines(std::vector<GridLine>& out, float lo, float hi, float step, bool horizontal, const ViewFrame& frame) {
    const long first = static_cast<long>(std::floor(lo / step));
    const long last = static_cast<long>(std::ceil(hi / step));
    if (last - first > kMaxGridLines) {
        return;
    }
    for (long k = first; k <= last; ++k) {
        const float world = static_cast<float>(k) * step;
        const PixelPoint p = toPixel(frame, horizontal ? Vec3{0.0F, world, 0.0F} : Vec3{world, 0.0F, 0.0F});
        out.push_back({horizontal ? p.y : p.x, k % 10 == 0});
    }
}

}  // namespace

Grid buildGrid(const ViewFrame& frame, float width, float height, float step) {
    Grid grid;
    grid.step = effectiveGridStep(step, frame.scale);
    if (grid.step <= 0.0F || !(width > 0.0F) || !(height > 0.0F)) {
        return grid;
    }
    const Vec3 topLeft = toWorld(frame, 0.0F, 0.0F);
    const Vec3 bottomRight = toWorld(frame, width, height);
    addLines(grid.vertical, std::min(topLeft.x, bottomRight.x), std::max(topLeft.x, bottomRight.x), grid.step, false, frame);
    addLines(grid.horizontal, std::min(topLeft.y, bottomRight.y), std::max(topLeft.y, bottomRight.y), grid.step, true, frame);
    return grid;
}

}  // namespace whitehole::preview
