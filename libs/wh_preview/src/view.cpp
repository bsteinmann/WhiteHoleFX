// view.cpp: Weltkoordinaten <-> Bildpunkte, Treffertests, siehe view.hpp.
#include "whitehole/preview/view.hpp"

#include <algorithm>
#include <limits>

namespace whitehole::preview {

PixelPoint toPixel(const ViewFrame& frame, const Vec3& world) noexcept {
    const float shift = frame.oblique ? kObliqueShift * world.z : 0.0F;
    return PixelPoint{frame.offsetX + (world.x + shift - frame.minX) * frame.scale,
                      frame.offsetY - (world.y + shift - frame.minY) * frame.scale};
}

Vec3 toWorld(const ViewFrame& frame, float pixelX, float pixelY, float z) noexcept {
    const float shift = frame.oblique ? kObliqueShift * z : 0.0F;
    const float scale = frame.scale != 0.0F ? frame.scale : 1.0F;
    return Vec3{(pixelX - frame.offsetX) / scale + frame.minX - shift,
                (frame.offsetY - pixelY) / scale + frame.minY - shift, z};
}

PixelRect toPixels(const ViewFrame& frame, const mapping::Box& box) noexcept {
    PixelRect rect{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    for (int corner = 0; corner < 8; ++corner) {
        const Vec3 p{(corner & 1) != 0 ? box.max.x : box.min.x, (corner & 2) != 0 ? box.max.y : box.min.y,
                     (corner & 4) != 0 ? box.max.z : box.min.z};
        const PixelPoint q = toPixel(frame, p);
        rect.left = std::min(rect.left, q.x);
        rect.right = std::max(rect.right, q.x);
        rect.top = std::min(rect.top, q.y);
        rect.bottom = std::max(rect.bottom, q.y);
    }
    return rect;
}

mapping::Box worldBox(const mapping::Fixture& fixture) noexcept {
    const mapping::Box local = mapping::localBox(fixture.shape);
    const Vec3& o = fixture.origin;
    return mapping::Box{Vec3{o.x + local.min.x, o.y + local.min.y, o.z + local.min.z},
                        Vec3{o.x + local.max.x, o.y + local.max.y, o.z + local.max.z}};
}

std::optional<mapping::FixtureId> fixtureAt(const mapping::Layout& layout, const ViewFrame& frame, float pixelX, float pixelY,
                                   float tolerance) {
    std::optional<mapping::FixtureId> best;
    float bestArea = std::numeric_limits<float>::max();
    for (const mapping::Fixture& fixture : layout.fixtures()) {
        const PixelRect r = toPixels(frame, worldBox(fixture));
        if (pixelX < r.left - tolerance || pixelX > r.right + tolerance || pixelY < r.top - tolerance ||
            pixelY > r.bottom + tolerance) {
            continue;
        }
        const float area = (r.width() + 2.0F * tolerance) * (r.height() + 2.0F * tolerance);
        if (area <= bestArea) {
            bestArea = area;
            best = fixture.id;
        }
    }
    return best;
}

std::vector<mapping::FixtureId> fixturesInRect(const mapping::Layout& layout, const ViewFrame& frame, const PixelRect& rect) {
    std::vector<mapping::FixtureId> out;
    for (const mapping::Fixture& fixture : layout.fixtures()) {
        const PixelRect r = toPixels(frame, worldBox(fixture));
        if (r.right >= rect.left && r.left <= rect.right && r.bottom >= rect.top && r.top <= rect.bottom) {
            out.push_back(fixture.id);
        }
    }
    return out;
}

}  // namespace whitehole::preview
