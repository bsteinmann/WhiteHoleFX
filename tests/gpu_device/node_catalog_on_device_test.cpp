// Die Misch-, Modifier-, Transform- und Ausgangs-Nodes auf einem echten Gerät: Der erzeugte WGSL-Code
// läuft, und einzelne Pixel stimmen mit der von Hand gerechneten Erwartung überein. Ohne Adapter
// übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "builtin_lab.hpp"
#include "gpu_stage_support.hpp"

using namespace whitehole;
using namespace whitehole::gpu;
using whitehole::test::Lab;

namespace {

using namespace whitehole::gputest;

/// Das gerenderte Bild: `at(x, y)` liefert RGBA, y zählt von oben (Zeile 0 = oben).
struct Pixels {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<float> data;

    [[nodiscard]] std::array<float, 4> at(std::uint32_t x, std::uint32_t y) const {
        const std::size_t i = (std::size_t{y} * width + x) * 4;
        return {data[i], data[i + 1], data[i + 2], data[i + 3]};
    }
};

Pixels render(WebGpuDevice& device, Lab& lab, NodeId target, std::uint32_t width, std::uint32_t height,
             double seconds = 0.0) {
    executor::CanvasStageConfig config;
    config.extent = Extent3{width, height, 1};
    config.format = TextureFormat::Rgba32Float;
    config.target = graph::PortRef{target, "out"};
    auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
    REQUIRE(created.ok());
    REQUIRE(created.value()->setGraph(lab.graph).ok());
    const std::vector<std::byte> bytes = renderCanvas(device, *created.value(), config.extent, config.format, seconds);
    REQUIRE(bytes.size() == std::size_t{width} * height * 16U);
    Pixels image;
    image.width = width;
    image.height = height;
    image.data.resize(std::size_t{width} * height * 4);
    std::memcpy(image.data.data(), bytes.data(), bytes.size());
    CHECK(device.takeErrors().empty());
    return image;
}

bool closeTo(float a, float b, float tolerance = 2e-3F) { return std::abs(a - b) <= tolerance; }

bool isColor(const std::array<float, 4>& c, float r, float g, float b, float a = 1.0F) {
    return closeTo(c[0], r) && closeTo(c[1], g) && closeTo(c[2], b) && closeTo(c[3], a);
}

NodeId solid(Lab& lab, float r, float g, float b) {
    const NodeId node = lab.add("wh.generator.solid");
    lab.set(node, "color", ColorRgba{r, g, b, 1.0F});
    return node;
}

/// Verlauf in x von Schwarz nach Weiß: Rot = x der Pixelmitte.
NodeId rampX(Lab& lab) {
    const NodeId node = lab.add("wh.generator.gradient");
    lab.set(node, "angle", 0.0);
    return node;
}

float centerX(std::uint32_t x, std::uint32_t width) { return (static_cast<float>(x) + 0.5F) / static_cast<float>(width); }

}  // namespace

#define REQUIRE_DEVICE()                                      \
    const std::unique_ptr<WebGpuDevice> device = makeDevice(); \
    if (!device) {                                            \
        SKIP("Kein WebGPU-Adapter verfügbar");                \
    }

TEST_CASE("Ausgang: ganze Leinwand, Rechteck skaliert oder als Fenster, Helligkeit", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    const auto build = [&](Lab& lab, double mode) {
        const NodeId ramp = rampX(lab);
        const NodeId out = lab.add("wh.output.canvas");
        lab.set(out, "mode", static_cast<std::int64_t>(mode));
        lab.connect(ramp, "out", out, "source");
        return out;
    };
    {  // Standard: reicht das Bild unverändert durch
        Lab lab;
        const NodeId out = build(lab, 0);
        const Pixels image = render(*device, lab, out, 16, 4);
        for (std::uint32_t x = 0; x < 16; ++x) {
            INFO("x=" << x);
            REQUIRE(isColor(image.at(x, 1), centerX(x, 16), centerX(x, 16), centerX(x, 16)));
        }
    }
    {  // Rechteck x 0,25..0,75, Bild wird hineingeschoben
        Lab lab;
        const NodeId out = build(lab, 0);
        lab.set(out, "left", 0.25);
        lab.set(out, "width", 0.5);
        const Pixels image = render(*device, lab, out, 16, 4);
        CHECK(isColor(image.at(0, 0), 0.0F, 0.0F, 0.0F, 0.0F));
        CHECK(isColor(image.at(15, 0), 0.0F, 0.0F, 0.0F, 0.0F));
        const float expected = (centerX(6, 16) - 0.25F) / 0.5F;
        CHECK(closeTo(image.at(6, 2)[0], expected));
        CHECK(closeTo(image.at(6, 2)[3], 1.0F));
    }
    {  // Fenster: das Bild bleibt an seinem Platz
        Lab lab;
        const NodeId out = build(lab, 1);
        lab.set(out, "left", 0.25);
        lab.set(out, "width", 0.5);
        const Pixels image = render(*device, lab, out, 16, 4);
        CHECK(closeTo(image.at(0, 0)[3], 0.0F));
        CHECK(closeTo(image.at(6, 2)[0], centerX(6, 16)));
        CHECK(closeTo(image.at(6, 2)[3], 1.0F));
    }
    {  // nur die obere Hälfte; Helligkeit halbiert
        Lab lab;
        const NodeId out = build(lab, 1);
        lab.set(out, "height", 0.5);
        lab.set(out, "brightness", 0.5);
        const Pixels image = render(*device, lab, out, 8, 8);
        CHECK(closeTo(image.at(4, 1)[3], 1.0F));  // oben
        CHECK(closeTo(image.at(4, 6)[3], 0.0F));  // unten
        CHECK(closeTo(image.at(4, 1)[0], centerX(4, 8) * 0.5F));
    }
}

TEST_CASE("Blend: alle Modi auf deckenden Farben", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    struct Expect {
        int mode;
        float r, g, b;
    };
    // A = (0,5; 0,5; 0,5), B = (0,25; 0,5; 1)
    const Expect expected[] = {
        {0, 0.75F, 1.0F, 1.0F},    // Add
        {1, 0.125F, 0.25F, 0.5F},  // Multiply
        {2, 0.625F, 0.75F, 1.0F},  // Screen
        {3, 0.25F, 0.0F, 0.0F},    // Subtract
        {4, 0.25F, 0.0F, 0.5F},    // Difference
        {5, 0.5F, 0.5F, 1.0F},     // Lighten
        {6, 0.25F, 0.5F, 0.5F},    // Darken
        {7, 0.25F, 0.5F, 1.0F},    // Over
    };
    for (const Expect& e : expected) {
        Lab lab;
        const NodeId a = solid(lab, 0.5F, 0.5F, 0.5F);
        const NodeId b = solid(lab, 0.25F, 0.5F, 1.0F);
        const NodeId blend = lab.add("wh.blend.blend");
        lab.set(blend, "mode", static_cast<std::int64_t>(e.mode));
        lab.connect(a, "out", blend, "a");
        lab.connect(b, "out", blend, "b");
        const Pixels image = render(*device, lab, blend, 2, 2);
        INFO("mode " << e.mode);
        CHECK(isColor(image.at(0, 0), e.r, e.g, e.b));
    }
    Lab lab;  // Amount 0,5 mit Over: halb A, halb B
    const NodeId a = solid(lab, 0.5F, 0.5F, 0.5F);
    const NodeId b = solid(lab, 0.25F, 0.5F, 1.0F);
    const NodeId blend = lab.add("wh.blend.blend");
    lab.set(blend, "mode", std::int64_t{7});
    lab.set(blend, "amount", 0.5);
    lab.connect(a, "out", blend, "a");
    lab.connect(b, "out", blend, "b");
    CHECK(isColor(render(*device, lab, blend, 2, 2).at(1, 1), 0.375F, 0.5F, 0.75F));
}

TEST_CASE("Crossfade: linear und progressiv", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    const auto run = [&](std::int64_t mode, double amount) {
        Lab lab;
        const NodeId red = solid(lab, 1.0F, 0.0F, 0.0F);
        const NodeId blue = solid(lab, 0.0F, 0.0F, 1.0F);
        const NodeId fade = lab.add("wh.blend.crossfade");
        lab.set(fade, "mode", mode);
        lab.set(fade, "amount", amount);
        lab.connect(red, "out", fade, "a");
        lab.connect(blue, "out", fade, "b");
        return render(*device, lab, fade, 2, 2).at(0, 0);
    };
    CHECK(isColor(run(0, 0.25), 0.75F, 0.0F, 0.25F));
    CHECK(isColor(run(1, 0.25), 1.0F, 0.0F, 0.5F));  // A voll, B halb
    CHECK(isColor(run(1, 0.5), 1.0F, 0.0F, 1.0F));   // beide voll
    CHECK(isColor(run(1, 1.0), 0.0F, 0.0F, 1.0F));
}

TEST_CASE("Mask und Key: Helligkeit bestimmt die Deckkraft", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    const auto masked = [&](std::int64_t mode, bool invert) {
        Lab lab;
        const NodeId content = solid(lab, 1.0F, 0.0F, 0.0F);
        const NodeId mask = rampX(lab);
        const NodeId node = lab.add("wh.blend.mask");
        lab.set(node, "mode", mode);
        lab.set(node, "threshold", 0.5);
        lab.set(node, "softness", 0.0);
        lab.set(node, "invert", invert);
        lab.connect(content, "out", node, "content");
        lab.connect(mask, "out", node, "mask");
        return render(*device, lab, node, 16, 2);
    };
    const Pixels intensity = masked(1, false);
    CHECK(closeTo(intensity.at(2, 0)[3], centerX(2, 16)));
    CHECK(closeTo(intensity.at(13, 0)[3], centerX(13, 16)));
    CHECK(closeTo(intensity.at(13, 0)[0], 1.0F));  // Farbe vom Inhalt
    const Pixels shape = masked(0, false);
    CHECK(closeTo(shape.at(2, 0)[3], 0.0F));
    CHECK(closeTo(shape.at(13, 0)[3], 1.0F));
    const Pixels inverted = masked(0, true);
    CHECK(closeTo(inverted.at(2, 0)[3], 1.0F));
    CHECK(closeTo(inverted.at(13, 0)[3], 0.0F));

    Lab lab;
    const NodeId ramp = rampX(lab);
    const NodeId key = lab.add("wh.blend.key");
    lab.set(key, "threshold", 0.5);
    lab.set(key, "softness", 0.0);
    lab.connect(ramp, "out", key, "source");
    const Pixels keyed = render(*device, lab, key, 16, 2);
    CHECK(closeTo(keyed.at(2, 0)[3], 0.0F));
    CHECK(closeTo(keyed.at(13, 0)[3], 1.0F));
}

TEST_CASE("Color Adjust: Farbton, Sättigung, Helligkeit, Gamma, Invertieren", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    const auto adjust = [&](ColorRgba color, const char* key, auto value) {
        Lab lab;
        const NodeId source = lab.add("wh.generator.solid");
        lab.set(source, "color", color);
        const NodeId node = lab.add("wh.modifier.coloradjust");
        lab.set(node, key, value);
        lab.connect(source, "out", node, "source");
        return render(*device, lab, node, 2, 2).at(0, 0);
    };
    const ColorRgba red{1.0F, 0.0F, 0.0F, 1.0F};
    CHECK(isColor(adjust(red, "hue", 1.0 / 3.0), 0.0F, 1.0F, 0.0F));
    CHECK(isColor(adjust(red, "saturation", 0.0), 1.0F, 1.0F, 1.0F));
    CHECK(isColor(adjust(red, "brightness", 0.5), 0.5F, 0.0F, 0.0F));
    CHECK(isColor(adjust(red, "invert", true), 0.0F, 1.0F, 1.0F));
    CHECK(isColor(adjust(ColorRgba{0.25F, 0.25F, 0.25F, 1.0F}, "gamma", 2.0), 0.5F, 0.5F, 0.5F));
}

TEST_CASE("Strobe: leuchtet nur in der Einschaltphase", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    Lab lab;
    const NodeId white = solid(lab, 1.0F, 1.0F, 1.0F);
    const NodeId strobe = lab.add("wh.modifier.strobe");
    lab.set(strobe, "rate", 1.0);
    lab.set(strobe, "duty", 0.5);
    lab.set(strobe, "fade", 0.0);
    lab.connect(white, "out", strobe, "source");
    CHECK(closeTo(render(*device, lab, strobe, 2, 2, 0.25).at(0, 0)[3], 1.0F));
    CHECK(closeTo(render(*device, lab, strobe, 2, 2, 0.75).at(0, 0)[3], 0.0F));
    lab.set(strobe, "fade", 0.5);  // nach dem Blitz klingt es über die halbe Periode aus
    CHECK(closeTo(render(*device, lab, strobe, 2, 2, 0.75).at(0, 0)[3], 0.5F));
}

TEST_CASE("Slide, Tile und Pixelate bewegen die Abtastkoordinate", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    {  // Slide um 0,25 nach rechts: links davon ist es durchsichtig
        Lab lab;
        const NodeId ramp = rampX(lab);
        const NodeId slide = lab.add("wh.transform.slide");
        lab.set(slide, "x", 0.25);
        lab.connect(ramp, "out", slide, "source");
        const Pixels image = render(*device, lab, slide, 16, 2);
        CHECK(closeTo(image.at(1, 0)[3], 0.0F));
        CHECK(closeTo(image.at(8, 0)[0], centerX(8, 16) - 0.25F));
        lab.set(slide, "wrap", true);
        const Pixels wrapped = render(*device, lab, slide, 16, 2);
        CHECK(closeTo(wrapped.at(0, 0)[0], centerX(0, 16) - 0.25F + 1.0F));
        CHECK(closeTo(wrapped.at(0, 0)[3], 1.0F));
    }
    {  // Tile 2x: dieselbe Kachel zweimal, gespiegelt jede zweite
        Lab lab;
        const NodeId ramp = rampX(lab);
        const NodeId tile = lab.add("wh.transform.tile");
        lab.set(tile, "countX", 2.0);
        lab.set(tile, "countY", 1.0);
        lab.connect(ramp, "out", tile, "source");
        const Pixels image = render(*device, lab, tile, 16, 2);
        CHECK(closeTo(image.at(2, 0)[0], 0.3125F));
        CHECK(closeTo(image.at(10, 0)[0], 0.3125F));
        lab.set(tile, "mirror", true);
        const Pixels mirrored = render(*device, lab, tile, 16, 2);
        CHECK(closeTo(mirrored.at(10, 0)[0], 0.6875F));
    }
    {  // Pixelate: vier Zellen
        Lab lab;
        const NodeId ramp = rampX(lab);
        const NodeId pixel = lab.add("wh.transform.pixelate");
        lab.set(pixel, "cellsX", 4.0);
        lab.set(pixel, "cellsY", 1.0);
        lab.connect(ramp, "out", pixel, "source");
        const Pixels image = render(*device, lab, pixel, 16, 2);
        CHECK(closeTo(image.at(0, 0)[0], 0.125F));
        CHECK(closeTo(image.at(3, 0)[0], 0.125F));
        CHECK(closeTo(image.at(5, 0)[0], 0.375F));
        CHECK(closeTo(image.at(15, 0)[0], 0.875F));
    }
}

TEST_CASE("Scan Line, Outline, Ripples und Fire", "[gpu_device][nodes]") {
    REQUIRE_DEVICE();
    {
        Lab lab;
        const NodeId scan = lab.add("wh.generator.scan");
        lab.set(scan, "position", 0.5);
        lab.set(scan, "width", 0.1);
        lab.set(scan, "softness", 1.0);
        const Pixels image = render(*device, lab, scan, 16, 2);
        CHECK(closeTo(image.at(0, 0)[3], 0.0F));
        CHECK(image.at(7, 0)[3] > 0.5F);
    }
    {  // Ein heller Balken in der Mitte bekommt einen roten Rand
        Lab lab;
        const NodeId scan = lab.add("wh.generator.scan");
        lab.set(scan, "position", 0.5);
        lab.set(scan, "width", 0.05);
        lab.set(scan, "softness", 0.01);
        const NodeId outline = lab.add("wh.modifier.outline");
        lab.set(outline, "width", 0.06);
        lab.set(outline, "color", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
        lab.connect(scan, "out", outline, "source");
        const Pixels image = render(*device, lab, outline, 32, 2);
        CHECK(isColor(image.at(18, 0), 1.0F, 0.0F, 0.0F, 1.0F));  // neben dem Balken: Rand
        CHECK(closeTo(image.at(30, 0)[3], 0.0F));                    // weit weg: nichts
        CHECK(closeTo(image.at(16, 0)[0], 1.0F));                    // im Balken: weiß
        CHECK(closeTo(image.at(16, 0)[1], 1.0F));
    }
    {  // Ripples: irgendwo ein Ring, alles im gültigen Bereich
        Lab lab;
        const NodeId ripples = lab.add("wh.generator.ripples");
        const Pixels image = render(*device, lab, ripples, 32, 32, 0.5);
        float most = 0.0F;
        for (std::uint32_t i = 0; i < image.data.size(); i += 4) {
            REQUIRE(image.data[i + 3] >= 0.0F);
            REQUIRE(image.data[i + 3] <= 1.0F + 1e-4F);
            most = std::max(most, image.data[i + 3]);
        }
        CHECK(most > 0.1F);
    }
    {  // Fire: deckend, unten heißer als oben
        Lab lab;
        const NodeId fire = lab.add("wh.generator.fire");
        const Pixels image = render(*device, lab, fire, 16, 16, 1.0);
        float bottom = 0.0F;
        float top = 0.0F;
        for (std::uint32_t x = 0; x < 16; ++x) {
            CHECK(closeTo(image.at(x, 0)[3], 1.0F));
            bottom += image.at(x, 15)[0] + image.at(x, 14)[0];
            top += image.at(x, 0)[0] + image.at(x, 1)[0];
        }
        CHECK(bottom > top);
    }
}
