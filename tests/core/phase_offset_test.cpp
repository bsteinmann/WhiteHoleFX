// Tests von PhaseOffset: bitgleich zu Zeit * Rate bei unberührter Rate, stetig beim Ändern der Rate.
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "whitehole/core/phase_offset.hpp"

using whitehole::PhaseOffset;

TEST_CASE("PhaseOffset: unberührte Rate ist bitgleich Zeit mal Rate") {
    PhaseOffset phase;
    for (const double t : {0.0, 0.016, 1.0 / 3.0, 12.5, 86400.1}) {
        CHECK(phase.phase(t, 3.7) == t * 3.7);
    }
}

TEST_CASE("PhaseOffset: Rate ändern lässt die Phase im selben Augenblick gleich") {
    PhaseOffset phase;
    CHECK(phase.phase(10.0, 2.0) == 20.0);
    CHECK(phase.phase(10.0, 5.0) == 20.0);
    CHECK(phase.phase(12.0, 5.0) == 30.0);
    CHECK(phase.phase(12.0, 0.0) == 30.0);
    CHECK(phase.phase(20.0, 0.0) == 30.0);
}

TEST_CASE("PhaseOffset: Zeit darf springen, nicht endliche Werte ändern nichts") {
    PhaseOffset phase;
    (void)phase.phase(10.0, 2.0);
    (void)phase.phase(10.0, 5.0);
    CHECK(phase.phase(3.0, 5.0) == -15.0);
    CHECK(phase.phase(12.0, 5.0) == 30.0);
    (void)phase.phase(5.0, std::numeric_limits<double>::quiet_NaN());
    CHECK(phase.phase(12.0, 5.0) == 30.0);
}
