// Spectrum Bars auf einem echten Gerät: Der erzeugte WGSL-Code (Spektrum als array<vec4<f32>, 8> im
// Uniform-Puffer, dynamisch indiziert) läuft, und die Balkenhöhen stimmen mit der Erwartung überein.
// Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include "node_render_support.hpp"

using namespace whitehole;
using namespace whitehole::gputest::nodes;

namespace {

/// Spektrum-Node -> Spectrum Bars (4 Balken ohne Lücke, weiß), Spektrum im Speicher gesetzt.
NodeId barsWith(Lab& lab, std::int64_t mode) {
    const NodeId spectrum = lab.add("wh.value.audiospectrum");
    lab.set(spectrum, "attack", 0.0);
    const NodeId bars = lab.add("wh.generator.spectrum");
    lab.set(bars, "bars", std::int64_t{4});
    lab.set(bars, "gap", 0.0);
    lab.set(bars, "softness", 0.0001);
    lab.set(bars, "mode", mode);
    lab.set(bars, "color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.set(bars, "colorTop", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    lab.connect(spectrum, "spectrum", bars, "spectrum");

    audio::AudioFrame frame;
    frame.frameCount = 1;
    frame.spectrum.bands[3] = 0.5F;    // Balken 0 (Bänder 0..7)
    frame.spectrum.bands[8] = 1.0F;    // Balken 1 (Bänder 8..15)
    frame.spectrum.bands[31] = 0.25F;  // Balken 3 (Bänder 24..31); Balken 2 bleibt still
    lab.audio->write(frame);
    return bars;
}

}  // namespace

TEST_CASE("Spectrum Bars: Balken wachsen von unten, Höhe folgt dem Band", "[gpu_device][nodes][audio]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId bars = barsWith(lab, 0);
    const Pixels image = render(*device, lab, bars, 8, 8);

    const auto filled = [&](std::uint32_t x, std::uint32_t y) { return image.at(x, y)[3] > 0.5F; };
    // Balken 0 (Pixel 0..1): Pegel 0,5 -> untere vier Zeilen (Zeile 7 ist unten).
    for (std::uint32_t y = 0; y < 8; ++y) {
        INFO("Balken 0, Zeile " << y);
        CHECK(filled(0, y) == (y >= 4));
        CHECK(filled(1, y) == (y >= 4));
    }
    // Balken 1 (Pixel 2..3): Pegel 1,0 -> alle Zeilen.
    for (std::uint32_t y = 0; y < 8; ++y) {
        INFO("Balken 1, Zeile " << y);
        CHECK(filled(2, y));
    }
    // Balken 2 (Pixel 4..5): still -> durchsichtig.
    for (std::uint32_t y = 0; y < 8; ++y) {
        INFO("Balken 2, Zeile " << y);
        CHECK_FALSE(filled(4, y));
    }
    // Balken 3 (Pixel 6..7): Pegel 0,25 -> untere zwei Zeilen.
    for (std::uint32_t y = 0; y < 8; ++y) {
        INFO("Balken 3, Zeile " << y);
        CHECK(filled(6, y) == (y >= 6));
    }
}

TEST_CASE("Spectrum Bars: von der Mitte aus wachsen die Balken nach oben und unten", "[gpu_device][nodes][audio]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId bars = barsWith(lab, 1);
    const Pixels image = render(*device, lab, bars, 8, 8);
    // Balken 0, Pegel 0,5: symmetrisch um die Mitte, Zeilen 2..5.
    for (std::uint32_t y = 0; y < 8; ++y) {
        INFO("Zeile " << y);
        CHECK((image.at(0, y)[3] > 0.5F) == (y >= 2 && y <= 5));
    }
}

TEST_CASE("Spectrum Bars: ohne Signal bleibt alles durchsichtig", "[gpu_device][nodes][audio]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId bars = lab.add("wh.generator.spectrum");
    const Pixels image = render(*device, lab, bars, 8, 8);
    for (std::uint32_t i = 3; i < image.data.size(); i += 4) {
        REQUIRE(image.data[i] == 0.0F);
    }
}
