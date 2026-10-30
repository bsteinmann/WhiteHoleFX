// Die echten Stages auf einem echten Gerät: Der vom Feldcompiler erzeugte WGSL-Text wird
// ausgeführt und das Ergebnis mit der CPU-Rechnung verglichen. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "builtin_lab.hpp"
#include "whitehole/executor/canvas_render_stage.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::gpu;
using whitehole::test::Lab;

namespace {

std::unique_ptr<WebGpuDevice> makeDevice() {
    Result<std::unique_ptr<WebGpuDevice>> device = WebGpuDevice::create();
    if (!device.ok()) {
        WebGpuOptions software;
        software.forceFallbackAdapter = true;
        device = WebGpuDevice::create(software);
    }
    return device.ok() ? std::move(device.value()) : nullptr;
}

std::vector<std::byte> readBack(WebGpuDevice& device, BufferHandle buffer) {
    bool done = false;
    std::vector<std::byte> bytes;
    device.readbackAsync(buffer, [&](Result<std::span<const std::byte>> r) {
        done = true;
        REQUIRE(r.ok());
        bytes.assign(r.value().begin(), r.value().end());
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        device.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(done);
    return bytes;
}

BufferHandle readbackBuffer(WebGpuDevice& device, std::uint64_t size) {
    BufferDesc desc;
    desc.sizeBytes = size;
    desc.usage = Usage::MapRead | Usage::CopyDestination;
    Result<BufferHandle> r = device.createBuffer(desc);
    REQUIRE(r.ok());
    return r.value();
}

FrameTime frameAt(std::uint64_t frame, double seconds) {
    FrameTime time;
    time.frame = frame;
    time.time = Seconds{seconds};
    return time;
}

float gradientT(const Vec3& p, double angleDegrees) {
    const double a = angleDegrees * 3.14159265358979323846 / 180.0;
    const double t = (static_cast<double>(p.x) - 0.5) * std::cos(a) + (static_cast<double>(p.y) - 0.5) * std::sin(a) + 0.5;
    return static_cast<float>(std::clamp(t, 0.0, 1.0));
}

/// Rendert die Canvas-Stage einmal und liest die Textur als Zeilen ohne Auffüllung zurück.
std::vector<std::byte> renderCanvas(WebGpuDevice& device, executor::CanvasRenderStage& stage, Extent3 extent,
                                    TextureFormat format, double seconds) {
    const std::uint32_t rowBytes = paddedBytesPerRow(extent.width, format);
    const BufferHandle target = readbackBuffer(device, std::uint64_t{rowBytes} * extent.height);
    REQUIRE(device.submit([&](ICommandRecorder& r) {
                      stage.record(frameAt(0, seconds), r);
                      r.copyTextureToBuffer(stage.output(), target);
                  }).ok());
    const std::vector<std::byte> padded = readBack(device, target);
    device.destroy(target);

    const std::uint32_t texel = bytesPerTexel(format);
    std::vector<std::byte> packed;
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        const auto* row = padded.data() + std::size_t{y} * rowBytes;
        packed.insert(packed.end(), row, row + std::size_t{extent.width} * texel);
    }
    return packed;
}

}  // namespace

TEST_CASE("Canvas: Verlauf in Rgba32Float stimmt mit der CPU-Rechnung überein", "[gpu_device][stage]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    Lab lab;
    const NodeId gradient = lab.add("wh.generator.gradient");
    lab.set(gradient, "angle", 30.0);

    executor::CanvasStageConfig config;
    config.extent = Extent3{16, 8, 1};
    config.format = TextureFormat::Rgba32Float;
    config.target = graph::PortRef{gradient, "out"};
    auto created = executor::CanvasRenderStage::create(*device, lab.snippets, lab.kernels, config);
    REQUIRE(created.ok());
    REQUIRE(created.value()->setGraph(lab.graph).ok());

    const std::vector<std::byte> bytes = renderCanvas(*device, *created.value(), config.extent, config.format, 0.0);
    REQUIRE(bytes.size() == 16U * 8U * 16U);
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            float rgba[4];
            std::memcpy(rgba, bytes.data() + (std::size_t{y} * 16 + x) * 16, sizeof rgba);
            // Pixelmitte; y zeigt nach oben, die oberste Bildzeile liegt bei p.y nahe 1.
            const Vec3 p{(static_cast<float>(x) + 0.5F) / 16.0F, 1.0F - (static_cast<float>(y) + 0.5F) / 8.0F, 0.0F};
            const float expected = gradientT(p, 30.0);
            INFO("x=" << x << " y=" << y);
            REQUIRE(std::abs(rgba[0] - expected) < 1e-4F);
            REQUIRE(std::abs(rgba[1] - expected) < 1e-4F);
            REQUIRE(std::abs(rgba[2] - expected) < 1e-4F);
            REQUIRE(std::abs(rgba[3] - 1.0F) < 1e-6F);
        }
    }
    CHECK(device->takeErrors().empty());
}

TEST_CASE("Canvas: eine geänderte Farbe kommt ohne neue Pipeline an", "[gpu_device][stage]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    Lab lab;
    const NodeId gradient = lab.add("wh.generator.gradient");
    executor::CanvasStageConfig config;
    config.extent = Extent3{8, 4, 1};
    config.format = TextureFormat::Rgba8Unorm;
    config.target = graph::PortRef{gradient, "out"};
    auto created = executor::CanvasRenderStage::create(*device, lab.snippets, lab.kernels, config);
    REQUIRE(created.ok());
    executor::CanvasRenderStage& stage = *created.value();
    REQUIRE(stage.setGraph(lab.graph).ok());
    (void)renderCanvas(*device, stage, config.extent, config.format, 0.0);

    lab.set(gradient, "colorB", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    REQUIRE(stage.setGraph(lab.graph).ok());
    const std::vector<std::byte> bytes = renderCanvas(*device, stage, config.extent, config.format, 0.0);
    CHECK(stage.stats().pipelinesBuilt == 1U);

    for (std::uint32_t x = 0; x < 8; ++x) {
        const std::byte* px = bytes.data() + x * 4;  // oberste Zeile
        const float t = (static_cast<float>(x) + 0.5F) / 8.0F;
        INFO("x=" << x);
        CHECK(std::abs(std::to_integer<int>(px[0]) - static_cast<int>(std::lround(t * 255.0F))) <= 1);
        CHECK(std::to_integer<int>(px[1]) == 0);
        CHECK(std::to_integer<int>(px[2]) == 0);
        CHECK(std::to_integer<int>(px[3]) == 255);
    }
}

namespace {

struct SamplingRig {
    Lab lab;
    NodeId node;
    mapping::LedMap map;
    std::unique_ptr<executor::SamplingRenderStage> stage;
};

std::unique_ptr<SamplingRig> samplingRig(WebGpuDevice& device, const char* nodeType) {
    auto rig = std::make_unique<SamplingRig>();
    rig->node = rig->lab.add(nodeType);
    mapping::MatrixShape matrix;
    matrix.columns = 4;
    matrix.rows = 2;
    mapping::Layout layout;
    REQUIRE(layout.add("Panel", Vec3{}, matrix).ok());
    rig->map = layout.bake().value();

    executor::SamplingStageConfig config;
    config.target = graph::PortRef{rig->node, "out"};
    config.ledCapacity = 64;
    auto created = executor::SamplingRenderStage::create(device, rig->lab.snippets, rig->lab.kernels, config);
    REQUIRE(created.ok());
    rig->stage = std::move(created.value());
    REQUIRE(rig->stage->setMap(rig->map).ok());
    REQUIRE(rig->stage->setGraph(rig->lab.graph).ok());
    return rig;
}

std::vector<ColorRgba> renderSampling(WebGpuDevice& device, SamplingRig& rig, double seconds) {
    const BufferHandle target = readbackBuffer(device, rig.stage->outputSizeBytes());
    REQUIRE(device.submit([&](ICommandRecorder& r) {
                      rig.stage->record(frameAt(0, seconds), r);
                      r.copyBufferToBuffer(rig.stage->output(), target, rig.stage->outputSizeBytes());
                  }).ok());
    const std::vector<std::byte> bytes = readBack(device, target);
    device.destroy(target);
    std::vector<ColorRgba> colors(rig.map.size());
    REQUIRE(bytes.size() >= colors.size() * sizeof(ColorRgba));
    std::memcpy(colors.data(), bytes.data(), colors.size() * sizeof(ColorRgba));
    return colors;
}

}  // namespace

TEST_CASE("Abtastung: jede LED bekommt den Wert des Feldes an ihrer Position", "[gpu_device][stage]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto rig = samplingRig(*device, "wh.generator.gradient");
    rig->lab.set(rig->node, "angle", 30.0);
    REQUIRE(rig->stage->setGraph(rig->lab.graph).ok());

    const std::vector<ColorRgba> colors = renderSampling(*device, *rig, 0.0);
    REQUIRE(colors.size() == 8);
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const float expected = gradientT(rig->map.unit[i], 30.0);
        INFO("LED " << i);
        REQUIRE(std::abs(colors[i].r - expected) < 1e-4F);
        REQUIRE(std::abs(colors[i].g - expected) < 1e-4F);
        REQUIRE(std::abs(colors[i].b - expected) < 1e-4F);
        REQUIRE(std::abs(colors[i].a - 1.0F) < 1e-6F);
    }
    CHECK(device->takeErrors().empty());
}

TEST_CASE("Abtastung: die Zeit erreicht den Shader", "[gpu_device][stage]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    auto rig = samplingRig(*device, "wh.generator.wave");
    rig->lab.set(rig->node, "frequency", 0.0);  // die Phase hängt nur noch von der Zeit ab
    rig->lab.set(rig->node, "speed", 1.0);
    REQUIRE(rig->stage->setGraph(rig->lab.graph).ok());

    // w = 0,5 + 0,5 sin(-2 pi t): bei t = 0,25 gleich 0 (Farbe A), bei t = 0,75 gleich 1 (Farbe B).
    for (const ColorRgba& c : renderSampling(*device, *rig, 0.25)) {
        CHECK(c.r < 1e-3F);
    }
    for (const ColorRgba& c : renderSampling(*device, *rig, 0.75)) {
        CHECK(c.r > 1.0F - 1e-3F);
    }
}
