// Die Abtast-Stage auf einem echten Gerät: jede LED bekommt den Wert des Feldes an ihrer Position,
// und die Zeit erreicht den Shader. Ohne Adapter übersprungen. Der Canvas-Teil steht in
// stage_on_device_test.cpp (Dateigröße).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "builtin_lab.hpp"
#include "gpu_stage_support.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::gpu;
using whitehole::test::Lab;
using namespace whitehole::gputest;

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
