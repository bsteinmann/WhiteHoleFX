// Tests der Grundtypen: StrongId, IdGenerator und Result<T>.
#include <catch2/catch_test_macros.hpp>

#include <unordered_set>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"

using namespace whitehole;

TEST_CASE("StrongId: Standardwert ist ungültig, Vergleich und Hash arbeiten korrekt") {
    constexpr NodeId none{};
    constexpr NodeId first{1};
    constexpr NodeId second{2};

    CHECK_FALSE(none.valid());
    CHECK(first.valid());
    CHECK(first < second);
    CHECK(first == NodeId{1});

    const std::unordered_set<NodeId> ids{first, second};
    CHECK(ids.contains(second));
    CHECK_FALSE(ids.contains(none));
}

TEST_CASE("IdGenerator vergibt aufsteigende IDs und respektiert reserveUpTo") {
    IdGenerator<NodeId> generator;
    CHECK(generator.next() == NodeId{1});
    CHECK(generator.next() == NodeId{2});

    generator.reserveUpTo(NodeId{10});
    CHECK(generator.next() == NodeId{11});

    generator.reserveUpTo(NodeId{5});  // ein kleinerer Wert setzt nichts zurück
    CHECK(generator.next() == NodeId{12});
}

TEST_CASE("Result<T> trägt entweder Wert oder Fehler") {
    const Result<int> good{42};
    REQUIRE(good.ok());
    CHECK(good.value() == 42);

    const Result<int> bad{Error{ErrorCode::NotFound, "Node fehlt"}};
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::NotFound);
    CHECK(bad.error().message == "Node fehlt");
}

TEST_CASE("Result<void> unterscheidet Erfolg und Fehler") {
    const Result<void> success{};
    CHECK(success.ok());

    const Result<void> failure{Error{ErrorCode::CycleDetected, "Zyklus"}};
    CHECK_FALSE(failure.ok());
    CHECK(failure.error().code == ErrorCode::CycleDetected);
}

TEST_CASE("Extent3 unterscheidet 2D und 3D und zählt Texel") {
    constexpr Extent3 plane{64, 32, 1};
    constexpr Extent3 cube{16, 16, 16};

    CHECK_FALSE(plane.is3D());
    CHECK(cube.is3D());
    CHECK(plane.texelCount() == 2048U);
    CHECK(cube.texelCount() == 4096U);
}
