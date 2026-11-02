// Tests der Farbwahl-Regeln (Issue #20): Eine gewählte Farbe wird nie unbeabsichtigt durchsichtig.
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "whitehole/runtime/color_pick.hpp"

using namespace whitehole;

TEST_CASE("Farbwahl: der Dialog startet nie mit Alpha 0", "[color_pick]") {
    CHECK(runtime::colorPickerStart(ColorRgba{0.2F, 0.4F, 0.6F, 0.0F}).a == 1.0F);
    CHECK(runtime::colorPickerStart(ColorRgba{0.2F, 0.4F, 0.6F, 0.0F}).g == 0.4F);  // die Farbe bleibt
    CHECK(runtime::colorPickerStart(ColorRgba{0, 0, 0, 0.5F}).a == 0.5F);            // Halbdurchsichtig bleibt
    CHECK(runtime::colorPickerStart(ColorRgba{0, 0, 0, 1.0F}).a == 1.0F);
    CHECK(runtime::colorPickerStart(ColorRgba{0, 0, 0, std::numeric_limits<float>::quiet_NaN()}).a == 1.0F);
}

TEST_CASE("Farbwahl: das Ergebnis ist sichtbar", "[color_pick]") {
    CHECK(runtime::colorFromPicker(ColorRgba{1, 0, 0, 0.0F}).a == 1.0F);
    CHECK(runtime::colorFromPicker(ColorRgba{1, 0, 0, 0.25F}).a == 0.25F);
    CHECK(runtime::colorFromPicker(ColorRgba{1, 0, 0, 0.0F}).r == 1.0F);
}
