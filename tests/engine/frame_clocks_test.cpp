#include <catch2/catch_test_macros.hpp>

#include "whitehole/engine/frame_clocks.hpp"

using namespace whitehole;
using namespace whitehole::engine;

TEST_CASE("ManualFrameClock schreibt die Zeit pro Frame um genau einen Schritt fort") {
    ManualFrameClock clock{Seconds{0.5}};

    CHECK(clock.current().frame == 0U);
    CHECK(clock.current().time.count() == 0.0);

    clock.advance();
    clock.advance();
    const FrameTime third = clock.advance();

    CHECK(third.frame == 3U);
    CHECK(third.time.count() == 1.5);
    CHECK(third.delta.count() == 0.5);
    CHECK(clock.current().frame == 3U);
}

TEST_CASE("ManualFrameClock driftet auch nach vielen Frames nicht") {
    ManualFrameClock clock{Seconds{1.0 / 60.0}};
    FrameTime last{};
    for (int i = 0; i < 60; ++i) {
        last = clock.advance();
    }
    // Die Zeit entsteht aus Frame-Index * Schritt, nicht aus 60 Additionen.
    CHECK(last.time.count() == 60.0 * (1.0 / 60.0));
}

TEST_CASE("SteadyFrameClock ist monoton und zählt Frames") {
    SteadyFrameClock clock;

    const FrameTime first = clock.advance();
    const FrameTime second = clock.advance();

    CHECK(first.frame == 1U);
    CHECK(second.frame == 2U);
    CHECK(second.time >= first.time);
    CHECK(second.delta.count() >= 0.0);
}
