#include <catch2/catch_test_macros.hpp>

#include "whitehole/core/hash.hpp"

using whitehole::fnv1a64;

// Referenzwerte der FNV-1a-64-Spezifikation; constexpr, also schon zur Compilezeit geprüft.
static_assert(fnv1a64("") == 0xcbf29ce484222325ULL);
static_assert(fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
static_assert(fnv1a64("foobar") == 0x85944171f73967e8ULL);

TEST_CASE("fnv1a64 liefert die bekannten Referenzwerte") {
    CHECK(fnv1a64("") == 0xcbf29ce484222325ULL);
    CHECK(fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
    CHECK(fnv1a64("foobar") == 0x85944171f73967e8ULL);
}

TEST_CASE("fnv1a64 unterscheidet verschiedene Eingaben") {
    CHECK(fnv1a64("fn wh_f1()") != fnv1a64("fn wh_f2()"));
    CHECK(fnv1a64("ab") != fnv1a64("ba"));
}
