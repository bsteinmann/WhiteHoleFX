// fixture_snap_test.cpp: Tests zu preview/fixture_snap.hpp (Einrasten an Kanten und Mitten anderer Fixtures).
//
// Warum es diese Datei gibt: Das Einrasten darf nur eingreifen, wenn etwas nah genug ist, muss den kleinsten
// Abstand wählen und Hilfslinien an genau den Linien liefern, an denen es eingerastet ist.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>

#include "whitehole/preview/fixture_snap.hpp"

using Catch::Approx;
using whitehole::preview::FixtureSnap;
using whitehole::preview::snapToFixtures;
using whitehole::preview::SnapRect;

TEST_CASE("Fixture-Einrasten: nichts in der Nähe, nichts passiert", "[preview][snap]") {
    const std::array<SnapRect, 1> others{SnapRect{10.0F, 10.0F, 20.0F, 20.0F}};
    const FixtureSnap s = snapToFixtures({0.0F, 0.0F, 4.0F, 4.0F}, others, 0.5F);
    CHECK_FALSE(s.snappedX);
    CHECK_FALSE(s.snappedY);
    CHECK(s.guides.empty());
    CHECK_FALSE(snapToFixtures({0.0F, 0.0F, 4.0F, 4.0F}, {}, 0.5F).snappedX);
    CHECK_FALSE(snapToFixtures({0.0F, 0.0F, 4.0F, 4.0F}, others, 0.0F).snappedX);
}

TEST_CASE("Fixture-Einrasten: Kante an Kante rastet ein und zeigt die Linie", "[preview][snap]") {
    const std::array<SnapRect, 1> others{SnapRect{0.0F, 0.0F, 10.0F, 5.0F}};
    // Rechte Kante des bewegten Rechtecks bei 10.2 + ... : seine linke Kante (10.3) liegt 0.3 neben der rechten Kante (10.0).
    const FixtureSnap s = snapToFixtures({10.3F, 0.2F, 14.3F, 3.2F}, others, 0.5F);
    REQUIRE(s.snappedX);
    CHECK(s.dx == Approx(-0.3F));
    REQUIRE(s.snappedY);
    CHECK(s.dy == Approx(-0.2F));  // untere Kanten bündig (0.2 -> 0)
    bool vertical10 = false;
    for (const auto& g : s.guides) {
        if (g.vertical && g.position == Approx(10.0F)) {
            vertical10 = true;
            CHECK(g.from == Approx(0.0F));
            CHECK(g.to == Approx(5.0F));
        }
    }
    CHECK(vertical10);
}

TEST_CASE("Fixture-Einrasten: Mitte auf Mitte", "[preview][snap]") {
    const std::array<SnapRect, 1> others{SnapRect{0.0F, 0.0F, 20.0F, 10.0F}};  // Mitte x = 10
    const FixtureSnap s = snapToFixtures({8.2F, 30.0F, 11.2F, 33.0F}, others, 0.5F);  // Mitte x = 9.7
    REQUIRE(s.snappedX);
    CHECK(s.dx == Approx(0.3F));
    CHECK_FALSE(s.snappedY);
}

TEST_CASE("Fixture-Einrasten: der kleinste Abstand gewinnt", "[preview][snap]") {
    const std::array<SnapRect, 2> others{SnapRect{0.0F, 100.0F, 10.0F, 110.0F}, SnapRect{10.4F, 200.0F, 20.0F, 210.0F}};
    // linke Kante bei 10.1: 0.1 von der rechten Kante des ersten, 0.3 von der linken des zweiten.
    const FixtureSnap s = snapToFixtures({10.1F, 0.0F, 15.0F, 4.0F}, others, 0.5F);
    REQUIRE(s.snappedX);
    CHECK(s.dx == Approx(-0.1F));
}

TEST_CASE("Fixture-Einrasten: Hilfslinien nur an den Linien, an denen eingerastet wurde", "[preview][snap]") {
    // Zwei Nachbarn mit derselben Oberkante: beide bekommen eine waagrechte Linie bei y = 5.
    const std::array<SnapRect, 2> others{SnapRect{0.0F, 0.0F, 4.0F, 5.0F}, SnapRect{20.0F, 1.0F, 24.0F, 5.0F}};
    const FixtureSnap s = snapToFixtures({8.0F, 2.0F, 12.0F, 5.1F}, others, 0.3F);
    REQUIRE(s.snappedY);
    int horizontal = 0;
    for (const auto& g : s.guides) {
        horizontal += (!g.vertical && g.position == Approx(5.0F)) ? 1 : 0;
    }
    CHECK(horizontal == 2);
}
