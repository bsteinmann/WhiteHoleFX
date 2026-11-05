// Tests von Verkabelung und LED-Auswahl in der 3D-Ansicht sowie der LED-Adresse.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/output/led_address.hpp"
#include "whitehole/preview/wiring.hpp"

using namespace whitehole;

namespace {

mapping::LedMap strip(std::uint32_t count) {
    mapping::Layout layout;
    mapping::StripShape s;
    s.count = count;
    s.end = Vec3{static_cast<float>(count - 1), 0.0F, 0.0F};
    REQUIRE(layout.add("Band", Vec3{0.0F, 0.0F, 0.0F}, s).ok());
    auto map = layout.bake();
    REQUIRE(map.ok());
    return map.value();
}

}  // namespace

TEST_CASE("Die Verkabelung folgt der Kabelreihenfolge", "[preview][wiring]") {
    const mapping::LedMap map = strip(10);
    const preview::Camera3d camera = preview::cameraFor(preview::ViewPreset::Front);
    const auto paths = preview::projectWiring(map, camera, 400, 100, 20, {});
    REQUIRE(paths.size() == 1);
    REQUIRE(paths[0].dots.size() == 10);
    for (std::size_t i = 1; i < paths[0].dots.size(); ++i) {
        CHECK(paths[0].dots[i].x > paths[0].dots[i - 1].x);  // in der Frontansicht läuft x nach rechts
    }
}

TEST_CASE("pickLed findet die nächste LED und nichts in der Ferne", "[preview][wiring]") {
    const mapping::LedMap map = strip(10);
    const preview::Camera3d camera = preview::cameraFor(preview::ViewPreset::Front);
    const auto paths = preview::projectWiring(map, camera, 400, 100, 20, {});
    const preview::WireDot dot = paths[0].dots[4];
    const auto hit = preview::pickLed(map, camera, 400, 100, 20, dot.x + 1.0F, dot.y - 1.0F, 12.0F, {});
    REQUIRE(hit.has_value());
    CHECK(hit->indexInFixture == 4);
    CHECK(hit->led == 4);
    CHECK_FALSE(preview::pickLed(map, camera, 400, 100, 20, 200.0F, 95.0F, 3.0F, {}).has_value());
}

TEST_CASE("LED-Adresse: Universe-Wechsel nach 170 LEDs", "[output][address]") {
    output::FixturePatch patch;
    patch.protocol = output::Protocol::Sacn;
    patch.universe = 1;
    patch.channel = 1;
    const auto first = output::ledAddress(patch, 0);
    REQUIRE(first.has_value());
    CHECK(first->universe == 1);
    CHECK(first->firstChannel == 1);
    CHECK(first->lastChannel == 3);
    const auto last = output::ledAddress(patch, 169);
    CHECK(last->universe == 1);
    CHECK(last->firstChannel == 508);
    const auto next = output::ledAddress(patch, 170);
    CHECK(next->universe == 2);
    CHECK(next->firstChannel == 1);

    patch.channel = 100;  // 412 Kanäle frei → 137 LEDs im ersten Universe
    CHECK(output::ledAddress(patch, 136)->universe == 1);
    const auto wrapped = output::ledAddress(patch, 137);
    CHECK(wrapped->universe == 2);
    CHECK(wrapped->firstChannel == 1);

    patch.protocol = output::Protocol::Ddp;
    patch.ddpOffset = 30;
    CHECK(output::ledAddress(patch, 2)->firstChannel == 36);
}
