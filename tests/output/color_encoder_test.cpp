// ColorEncoder: Alpha, Helligkeit, Gamma, Rundung und Kanalreihenfolge.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "whitehole/output/color_encoder.hpp"

using namespace whitehole;
using namespace whitehole::output;

namespace {

using Bytes = std::array<std::uint8_t, 3>;

ColorEncoder make(ColorOrder order, float gamma, float brightness) {
    ColorSettings s;
    s.order = order;
    s.gamma = gamma;
    s.brightness = brightness;
    Result<ColorEncoder> r = ColorEncoder::create(s);
    REQUIRE(r.ok());
    return std::move(r.value());
}

}  // namespace

TEST_CASE("Mit Gamma 1 und voller Helligkeit wird nur auf 8 Bit gerundet", "[output][color]") {
    const ColorEncoder e = make(ColorOrder::RGB, 1.0F, 1.0F);
    CHECK(e.encode({0.0F, 0.0F, 0.0F, 1.0F}) == Bytes{0, 0, 0});
    CHECK(e.encode({1.0F, 1.0F, 1.0F, 1.0F}) == Bytes{255, 255, 255});
    CHECK(e.encode({0.2F, 0.5F, 1.0F, 1.0F}) == Bytes{51, 128, 255});
}

TEST_CASE("Jede Kanalreihenfolge legt die Bytes richtig", "[output][color]") {
    const ColorRgba c{0.2F, 0.5F, 1.0F, 1.0F};  // r=51, g=128, b=255
    CHECK(make(ColorOrder::RGB, 1.0F, 1.0F).encode(c) == Bytes{51, 128, 255});
    CHECK(make(ColorOrder::RBG, 1.0F, 1.0F).encode(c) == Bytes{51, 255, 128});
    CHECK(make(ColorOrder::GRB, 1.0F, 1.0F).encode(c) == Bytes{128, 51, 255});
    CHECK(make(ColorOrder::GBR, 1.0F, 1.0F).encode(c) == Bytes{128, 255, 51});
    CHECK(make(ColorOrder::BRG, 1.0F, 1.0F).encode(c) == Bytes{255, 51, 128});
    CHECK(make(ColorOrder::BGR, 1.0F, 1.0F).encode(c) == Bytes{255, 128, 51});
}

TEST_CASE("Gamma potenziert die Werte", "[output][color]") {
    const ColorEncoder e = make(ColorOrder::RGB, 2.0F, 1.0F);
    CHECK(e.encode({0.5F, 0.5F, 0.5F, 1.0F}) == Bytes{64, 64, 64});  // 0,25 * 255 = 63,75
    CHECK(e.encode({0.1F, 0.1F, 0.1F, 1.0F}) == Bytes{3, 3, 3});     // 0,01 * 255 = 2,55
    CHECK(e.encode({1.0F, 1.0F, 1.0F, 1.0F}) == Bytes{255, 255, 255});
    CHECK(e.encode({0.0F, 0.0F, 0.0F, 1.0F}) == Bytes{0, 0, 0});
}

TEST_CASE("Helligkeit und Alpha skalieren vor dem Gamma", "[output][color]") {
    const ColorRgba white{1.0F, 1.0F, 1.0F, 1.0F};
    CHECK(make(ColorOrder::RGB, 1.0F, 0.2F).encode(white) == Bytes{51, 51, 51});
    CHECK(make(ColorOrder::RGB, 1.0F, 0.0F).encode(white) == Bytes{0, 0, 0});

    const ColorRgba halfAlpha{1.0F, 1.0F, 1.0F, 0.2F};
    CHECK(make(ColorOrder::RGB, 1.0F, 1.0F).encode(halfAlpha) == Bytes{51, 51, 51});
    // Beides zusammen: 0,5 * 0,5 = 0,25, dann Gamma 2 -> 0,0625 * 255 = 15,9
    CHECK(make(ColorOrder::RGB, 2.0F, 0.5F).encode({1.0F, 1.0F, 1.0F, 0.5F}) == Bytes{16, 16, 16});
}

TEST_CASE("Ungültige Werte werden begrenzt", "[output][color]") {
    const ColorEncoder e = make(ColorOrder::RGB, 2.2F, 1.0F);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    CHECK(e.encode({2.0F, -1.0F, nan, 1.0F}) == Bytes{255, 0, 0});
    CHECK(e.encode({inf, -inf, 0.0F, 1.0F}) == Bytes{255, 0, 0});
    CHECK(e.encode({1.0F, 1.0F, 1.0F, nan}) == Bytes{0, 0, 0});
    CHECK(e.encode({1.0F, 1.0F, 1.0F, 5.0F}) == Bytes{255, 255, 255});
    CHECK(e.encode({1.0F, 1.0F, 1.0F, -1.0F}) == Bytes{0, 0, 0});
}

TEST_CASE("Die Kennlinie steigt monoton und trifft die Enden", "[output][color]") {
    for (const float gamma : {0.5F, 1.0F, 2.2F, 2.8F}) {
        const ColorEncoder e = make(ColorOrder::RGB, gamma, 1.0F);
        std::uint8_t last = 0;
        for (int i = 0; i <= 2000; ++i) {
            const float v = static_cast<float>(i) / 2000.0F;
            const std::uint8_t now = e.encode({v, v, v, 1.0F})[0];
            REQUIRE(now >= last);
            last = now;
        }
        CHECK(e.encode({0.0F, 0.0F, 0.0F, 1.0F})[0] == 0);
        CHECK(last == 255);
    }
}

TEST_CASE("Ungültige Einstellungen werden abgelehnt", "[output][color]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const float gamma : {0.0F, 0.09F, 10.1F, nan}) {
        ColorSettings s;
        s.gamma = gamma;
        const Result<ColorEncoder> r = ColorEncoder::create(s);
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().code == ErrorCode::InvalidArgument);
    }
    for (const float brightness : {-0.1F, 1.1F, nan}) {
        ColorSettings s;
        s.brightness = brightness;
        const Result<ColorEncoder> r = ColorEncoder::create(s);
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().code == ErrorCode::InvalidArgument);
    }
    // Die Grenzen selbst sind erlaubt.
    ColorSettings edge;
    edge.gamma = 0.1F;
    edge.brightness = 0.0F;
    CHECK(ColorEncoder::create(edge).ok());
    edge.gamma = 10.0F;
    edge.brightness = 1.0F;
    CHECK(ColorEncoder::create(edge).ok());
}
