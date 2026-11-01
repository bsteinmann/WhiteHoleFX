// volume_on_device_test.cpp: die 3D-Nodes und z-Regler auf einem echten Gerät, an einem LED-Gitter.
//
// Warum eine eigene Datei: Die anderen Node-Tests rendern eine 2D-Leinwand (z = 0). Für z braucht es LEDs im
// Raum; hier wird ein Gitter 3x2x4 abgetastet und jede LED mit einer Rechnung auf der CPU verglichen. Als
// Eingang dient "Position Test" (Rot = x, Grün = y, Blau = z), an dem sich die Abtastkoordinate direkt
// ablesen lässt. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <numbers>
#include <vector>

#include "builtin_lab.hpp"
#include "node_render_support.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::gpu;
using whitehole::test::Lab;
using namespace whitehole::gputest;

namespace {

struct CubeRig {
    Lab lab;
    mapping::LedMap map;
    std::unique_ptr<executor::SamplingRenderStage> stage;
};

std::unique_ptr<CubeRig> cubeRig() {
    auto rig = std::make_unique<CubeRig>();
    mapping::GridShape grid;
    grid.nx = 3;
    grid.ny = 2;
    grid.nz = 4;
    mapping::Layout layout;
    REQUIRE(layout.add("Cube", Vec3{}, grid).ok());
    rig->map = layout.bake().value();
    return rig;
}

/// Rendert das Feld am Node `target` für alle LEDs des Gitters.
std::vector<ColorRgba> sample(WebGpuDevice& device, CubeRig& rig, NodeId target, double seconds = 0.0) {
    executor::SamplingStageConfig config;
    config.target = graph::PortRef{target, "out"};
    config.ledCapacity = 64;
    auto created = executor::SamplingRenderStage::create(device, rig.lab.snippets, rig.lab.kernels, config);
    REQUIRE(created.ok());
    rig.stage = std::move(created.value());
    REQUIRE(rig.stage->setMap(rig.map).ok());
    REQUIRE(rig.stage->setGraph(rig.lab.graph).ok());
    const BufferHandle readback = readbackBuffer(device, rig.stage->outputSizeBytes());
    REQUIRE(device.submit([&](ICommandRecorder& r) {
                      rig.stage->record(frameAt(0, seconds), r);
                      r.copyBufferToBuffer(rig.stage->output(), readback, rig.stage->outputSizeBytes());
                  }).ok());
    const std::vector<std::byte> bytes = readBack(device, readback);
    device.destroy(readback);
    std::vector<ColorRgba> colors(rig.map.size());
    REQUIRE(bytes.size() >= colors.size() * sizeof(ColorRgba));
    std::memcpy(colors.data(), bytes.data(), colors.size() * sizeof(ColorRgba));
    CHECK(device.takeErrors().empty());
    return colors;
}

bool closeEnough(float a, float b, float tolerance = 2e-3F) { return std::abs(a - b) <= tolerance; }
float clamp01(float v) { return std::clamp(v, 0.0F, 1.0F); }

void expectRgb(const ColorRgba& c, float r, float g, float b, std::size_t led) {
    INFO("LED " << led << " ist (" << c.r << ", " << c.g << ", " << c.b << "), erwartet (" << r << ", " << g << ", " << b << ")");
    CHECK(closeEnough(c.r, r));
    CHECK(closeEnough(c.g, g));
    CHECK(closeEnough(c.b, b));
}

}  // namespace

TEST_CASE("Position Test: Rot, Grün und Blau sind x, y und z der LED", "[gpu_device][nodes][volume]") {
    REQUIRE_DEVICE();
    auto rig = cubeRig();
    const NodeId position = rig->lab.add("wh.generator.position");
    const std::vector<ColorRgba> colors = sample(*device, *rig, position);
    REQUIRE(colors.size() == 24U);
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const Vec3 p = rig->map.unit[i];
        expectRgb(colors[i], clamp01(p.x), clamp01(p.y), clamp01(p.z), i);
    }
}

TEST_CASE("Gradient und Scan Line: elevation 90 läuft entlang z", "[gpu_device][nodes][volume]") {
    REQUIRE_DEVICE();
    auto rig = cubeRig();
    const NodeId gradient = rig->lab.add("wh.generator.gradient");
    rig->lab.set(gradient, "elevation", 90.0);
    const std::vector<ColorRgba> colors = sample(*device, *rig, gradient);
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const float z = clamp01(rig->map.unit[i].z);
        expectRgb(colors[i], z, z, z, i);
    }
}

TEST_CASE("Box: innen die eine, außen die andere Farbe", "[gpu_device][nodes][volume]") {
    REQUIRE_DEVICE();
    auto rig = cubeRig();
    const NodeId box = rig->lab.add("wh.generator.box");
    rig->lab.set(box, "sizeX", 0.6);
    rig->lab.set(box, "sizeY", 2.0);
    rig->lab.set(box, "sizeZ", 0.6);
    rig->lab.set(box, "softness", 0.0);
    const std::vector<ColorRgba> colors = sample(*device, *rig, box);
    int inside = 0;
    int outside = 0;
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const Vec3 p = rig->map.unit[i];
        const bool in = std::abs(p.x - 0.5F) < 0.3F - 1e-3F && std::abs(p.z - 0.5F) < 0.3F - 1e-3F;
        const bool out = std::abs(p.x - 0.5F) > 0.3F + 1e-3F || std::abs(p.z - 0.5F) > 0.3F + 1e-3F;
        if (in) {
            expectRgb(colors[i], 1.0F, 1.0F, 1.0F, i);
            ++inside;
        } else if (out) {
            expectRgb(colors[i], 0.0F, 0.0F, 0.0F, i);
            ++outside;
        }
    }
    CHECK(inside > 0);
    CHECK(outside > 0);
}

TEST_CASE("Twist: Winkel 0 ändert nichts, 90 Grad dreht die Ebenen um die senkrechte Achse", "[gpu_device][nodes][volume]") {
    REQUIRE_DEVICE();
    auto rig = cubeRig();
    const NodeId position = rig->lab.add("wh.generator.position");
    const NodeId twist = rig->lab.add("wh.transform.twist");
    rig->lab.connect(position, "out", twist, "source");
    rig->lab.set(twist, "angle", 0.0);
    const std::vector<ColorRgba> same = sample(*device, *rig, twist);
    for (std::size_t i = 0; i < same.size(); ++i) {
        const Vec3 p = rig->map.unit[i];
        expectRgb(same[i], clamp01(p.x), clamp01(p.y), clamp01(p.z), i);
    }
    rig->lab.set(twist, "angle", 90.0);
    const std::vector<ColorRgba> turned = sample(*device, *rig, twist);
    for (std::size_t i = 0; i < turned.size(); ++i) {
        const Vec3 p = rig->map.unit[i];
        const float a = std::numbers::pi_v<float> / 2.0F * (p.y - 0.5F);
        const float dx = p.x - 0.5F;
        const float dz = p.z - 0.5F;
        const float rx = std::cos(a) * dx - std::sin(a) * dz + 0.5F;
        const float rz = std::sin(a) * dx + std::cos(a) * dz + 0.5F;
        expectRgb(turned[i], clamp01(rx), clamp01(p.y), clamp01(rz), i);
    }
}

TEST_CASE("Transform-Nodes mit z-Reglern: Scale, Slide, Pixelate und Region", "[gpu_device][nodes][volume]") {
    REQUIRE_DEVICE();
    auto rig = cubeRig();
    const NodeId position = rig->lab.add("wh.generator.position");

    SECTION("Scale: factorZ zoomt die Tiefe um die Mitte") {
        const NodeId scale = rig->lab.add("wh.transform.scale");
        rig->lab.connect(position, "out", scale, "source");
        rig->lab.set(scale, "factorZ", 2.0);
        const std::vector<ColorRgba> colors = sample(*device, *rig, scale);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            const Vec3 p = rig->map.unit[i];
            expectRgb(colors[i], clamp01(p.x), clamp01(p.y), clamp01((p.z - 0.5F) / 2.0F + 0.5F), i);
        }
    }
    SECTION("Pixelate: cellsZ = 2 rastert die Tiefe in zwei Schichten") {
        const NodeId pixelate = rig->lab.add("wh.transform.pixelate");
        rig->lab.connect(position, "out", pixelate, "source");
        rig->lab.set(pixelate, "cellsZ", 2.0);
        rig->lab.set(pixelate, "cellsX", 256.0);
        rig->lab.set(pixelate, "cellsY", 256.0);
        const std::vector<ColorRgba> colors = sample(*device, *rig, pixelate);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            const float z = rig->map.unit[i].z;
            const float layer = std::min(std::floor(z * 2.0F), 1.0F);
            INFO("LED " << i << " z=" << z);
            CHECK(closeEnough(colors[i].b, (layer + 0.5F) / 2.0F));
        }
    }
    SECTION("Slide: z verschiebt, der Rest bleibt durchsichtig") {
        const NodeId slide = rig->lab.add("wh.transform.slide");
        rig->lab.connect(position, "out", slide, "source");
        rig->lab.set(slide, "z", 0.25);
        const std::vector<ColorRgba> colors = sample(*device, *rig, slide);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            const float z = rig->map.unit[i].z - 0.25F;
            INFO("LED " << i);
            if (z > 0.01F) {
                CHECK(closeEnough(colors[i].b, clamp01(z)));
                CHECK(closeEnough(colors[i].a, 1.0F));
            } else if (z < -0.01F) {
                CHECK(closeEnough(colors[i].a, 0.0F));
            }
        }
    }
    SECTION("Region: zStart und zSize schneiden einen Quader aus") {
        const NodeId region = rig->lab.add("wh.transform.region");
        rig->lab.connect(position, "out", region, "source");
        rig->lab.set(region, "left", 0.0);
        rig->lab.set(region, "top", 0.0);
        rig->lab.set(region, "width", 1.0);
        rig->lab.set(region, "height", 1.0);
        rig->lab.set(region, "zStart", 0.25);
        rig->lab.set(region, "zSize", 0.5);
        const std::vector<ColorRgba> colors = sample(*device, *rig, region);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            const float k = (rig->map.unit[i].z - 0.25F) / 0.5F;
            INFO("LED " << i);
            if (k > 0.01F && k < 0.99F) {
                CHECK(closeEnough(colors[i].b, clamp01(k)));
            } else if (k < -0.01F || k > 1.01F) {
                CHECK(closeEnough(colors[i].a, 0.0F));
            }
        }
    }
}

TEST_CASE("Plasma: depth 0 ist in z überall gleich, depth > 0 unterscheidet die Schichten", "[gpu_device][nodes][volume]") {
    REQUIRE_DEVICE();
    auto rig = cubeRig();
    const NodeId plasma = rig->lab.add("wh.generator.plasma");
    const auto spread = [&](double depth) {
        rig->lab.set(plasma, "depth", depth);
        const std::vector<ColorRgba> colors = sample(*device, *rig, plasma, 0.5);
        float widest = 0.0F;
        for (std::size_t i = 0; i < colors.size(); ++i) {
            for (std::size_t j = 0; j < colors.size(); ++j) {
                const Vec3 a = rig->map.unit[i];
                const Vec3 b = rig->map.unit[j];
                if (closeEnough(a.x, b.x, 1e-4F) && closeEnough(a.y, b.y, 1e-4F)) {
                    widest = std::max(widest, std::abs(colors[i].r - colors[j].r) + std::abs(colors[i].g - colors[j].g));
                }
            }
        }
        return widest;
    };
    CHECK(spread(0.0) < 1e-4F);
    CHECK(spread(2.0) > 0.01F);
}
