// Tests: Rechteck des Region-Nodes mit der Maus ziehen (Rechnung ohne Oberfläche).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "whitehole/mapping/region_edit.hpp"

using namespace whitehole::mapping;
using Catch::Approx;

TEST_CASE("Bereich: Verschieben bleibt in der Leinwand", "[mapping][region]") {
    const Region start{0.1F, 0.2F, 0.3F, 0.4F};
    const Region moved = dragRegion(start, RegionHandle::Move, 0.1F, 0.1F);
    CHECK(moved.x == Approx(0.2F));
    CHECK(moved.y == Approx(0.3F));
    CHECK(moved.w == Approx(0.3F));
    CHECK(moved.h == Approx(0.4F));

    const Region far = dragRegion(start, RegionHandle::Move, 5.0F, -5.0F);
    CHECK(far.x == Approx(0.7F));  // rechter Rand 1.0
    CHECK(far.y == Approx(0.0F));
}

TEST_CASE("Bereich: Ecken ziehen, die Gegenecke bleibt", "[mapping][region]") {
    const Region start{0.2F, 0.2F, 0.4F, 0.4F};  // 0.2..0.6
    const Region br = dragRegion(start, RegionHandle::BottomRight, 0.1F, 0.2F);
    CHECK(br.x == Approx(0.2F));
    CHECK(br.y == Approx(0.2F));
    CHECK(br.w == Approx(0.5F));
    CHECK(br.h == Approx(0.6F));

    const Region tl = dragRegion(start, RegionHandle::TopLeft, -0.1F, -0.1F);
    CHECK(tl.x == Approx(0.1F));
    CHECK(tl.y == Approx(0.1F));
    CHECK(tl.x + tl.w == Approx(0.6F));
    CHECK(tl.y + tl.h == Approx(0.6F));

    const Region tr = dragRegion(start, RegionHandle::TopRight, 0.1F, 0.1F);
    CHECK(tr.x == Approx(0.2F));
    CHECK(tr.y == Approx(0.3F));
    CHECK(tr.w == Approx(0.5F));
    CHECK(tr.h == Approx(0.3F));

    const Region bl = dragRegion(start, RegionHandle::BottomLeft, -0.5F, 5.0F);
    CHECK(bl.x == Approx(0.0F));
    CHECK(bl.y + bl.h == Approx(1.0F));
    CHECK(bl.x + bl.w == Approx(0.6F));
}

TEST_CASE("Bereich: Ecke kann nicht über die Gegenecke hinaus", "[mapping][region]") {
    const Region start{0.2F, 0.2F, 0.4F, 0.4F};
    const Region r = dragRegion(start, RegionHandle::BottomRight, -0.9F, -0.9F);
    CHECK(r.w == Approx(kMinRegionSize));
    CHECK(r.h == Approx(kMinRegionSize));
    CHECK(r.x == Approx(0.2F));
    const Region l = dragRegion(start, RegionHandle::TopLeft, 0.9F, 0.9F);
    CHECK(l.x + l.w == Approx(0.6F));
    CHECK(l.w == Approx(kMinRegionSize));
}

TEST_CASE("Bereich: aus zwei Punkten aufspannen", "[mapping][region]") {
    const Region r = regionFromCorners(0.6F, 0.8F, 0.2F, 0.3F);  // beliebige Reihenfolge
    CHECK(r.x == Approx(0.2F));
    CHECK(r.y == Approx(0.3F));
    CHECK(r.w == Approx(0.4F));
    CHECK(r.h == Approx(0.5F));

    const Region outside = regionFromCorners(-1.0F, -1.0F, 2.0F, 2.0F);
    CHECK(outside == Region{0.0F, 0.0F, 1.0F, 1.0F});

    const Region point = regionFromCorners(1.0F, 1.0F, 1.0F, 1.0F);  // Klick ohne Ziehen
    CHECK(point.w == Approx(kMinRegionSize));
    CHECK(point.x + point.w == Approx(1.0F));
}
