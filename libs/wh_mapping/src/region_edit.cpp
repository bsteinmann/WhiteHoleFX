// region_edit.cpp: Ziehen des Effektbereichs, siehe region_edit.hpp.
#include "whitehole/mapping/region_edit.hpp"

#include <algorithm>

namespace whitehole::mapping {

namespace {

float clamp01(float v) noexcept { return std::clamp(v, 0.0F, 1.0F); }

/// Schiebt eine Kante `edge` so, dass der Abstand zu `fixed` mindestens kMinRegionSize beträgt und
/// beide in [0,1] bleiben.
float keepApart(float edge, float fixed, bool edgeIsLower) noexcept {
    edge = clamp01(edge);
    return edgeIsLower ? std::min(edge, fixed - kMinRegionSize) : std::max(edge, fixed + kMinRegionSize);
}

}  // namespace

Region dragRegion(const Region& start, RegionHandle handle, float dx, float dy) noexcept {
    if (handle == RegionHandle::Move) {
        Region r = start;
        r.x = std::clamp(start.x + dx, 0.0F, std::max(0.0F, 1.0F - start.w));
        r.y = std::clamp(start.y + dy, 0.0F, std::max(0.0F, 1.0F - start.h));
        return r;
    }
    float left = start.x;
    float top = start.y;
    float right = start.x + start.w;
    float bottom = start.y + start.h;
    const bool movesLeft = handle == RegionHandle::TopLeft || handle == RegionHandle::BottomLeft;
    const bool movesTop = handle == RegionHandle::TopLeft || handle == RegionHandle::TopRight;
    if (movesLeft) {
        left = keepApart(left + dx, right, true);
    } else {
        right = keepApart(right + dx, left, false);
    }
    if (movesTop) {
        top = keepApart(top + dy, bottom, true);
    } else {
        bottom = keepApart(bottom + dy, top, false);
    }
    return Region{left, top, right - left, bottom - top};
}

Region regionFromCorners(float x0, float y0, float x1, float y1) noexcept {
    float left = clamp01(std::min(x0, x1));
    float right = clamp01(std::max(x0, x1));
    float top = clamp01(std::min(y0, y1));
    float bottom = clamp01(std::max(y0, y1));
    if (right - left < kMinRegionSize) {
        right = std::min(1.0F, left + kMinRegionSize);
        left = right - kMinRegionSize;
    }
    if (bottom - top < kMinRegionSize) {
        bottom = std::min(1.0F, top + kMinRegionSize);
        top = bottom - kMinRegionSize;
    }
    return Region{left, top, right - left, bottom - top};
}

}  // namespace whitehole::mapping
