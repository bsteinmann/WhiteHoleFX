// stage_fixture.hpp: Prüfstand für die CanvasRenderStage: Built-in-Nodes, Fake-GPU, eine Stage.
// Der Graph ist Gradient -> Rotate; Rotate.out ist das Ziel.
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "builtin_lab.hpp"
#include "fake_gpu_device.hpp"
#include "whitehole/executor/canvas_render_stage.hpp"

namespace whitehole::test {

struct StageFixture {
    Lab lab;
    FakeGpuDevice device;
    NodeId gradient;
    NodeId rotate;
    std::unique_ptr<executor::CanvasRenderStage> stage;
    std::uint64_t frame{0};

    explicit StageFixture(Extent3 extent = Extent3{16, 8, 1},
                          gpu::TextureFormat format = gpu::TextureFormat::Rgba16Float) {
        gradient = lab.add("wh.generator.gradient");
        rotate = lab.add("wh.transform.rotate");
        lab.connect(gradient, "out", rotate, "source");

        executor::CanvasStageConfig config;
        config.extent = extent;
        config.format = format;
        config.target = target();
        auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE(created.ok());
        stage = std::move(created.value());
    }

    [[nodiscard]] graph::PortRef target() const { return graph::PortRef{rotate, "out"}; }

    /// Ein Frame: aufzeichnen und einreichen.
    void render(double seconds, double delta = 0.0) {
        FrameTime time;
        time.frame = frame++;
        time.time = Seconds{seconds};
        time.delta = Seconds{delta};
        REQUIRE(device.submit([&](gpu::ICommandRecorder& recorder) { stage->record(time, recorder); }).ok());
    }

    [[nodiscard]] const FakeGpuDevice::DispatchRecord& lastDispatch() const {
        REQUIRE_FALSE(device.dispatches().empty());
        return device.dispatches().back();
    }
};

inline gpu::BufferHandle bufferAt(const FakeGpuDevice::DispatchRecord& dispatch, std::uint32_t group,
                                  std::uint32_t slot) {
    for (const gpu::Binding& binding : dispatch.bindings) {
        if (binding.group == group && binding.slot == slot) {
            return std::get<gpu::BufferHandle>(binding.resource);
        }
    }
    FAIL("Binding (" << group << ", " << slot << ") fehlt");
    return gpu::BufferHandle{};
}

inline gpu::TextureHandle textureAt(const FakeGpuDevice::DispatchRecord& dispatch, std::uint32_t group,
                                    std::uint32_t slot) {
    for (const gpu::Binding& binding : dispatch.bindings) {
        if (binding.group == group && binding.slot == slot) {
            return std::get<gpu::TextureHandle>(binding.resource);
        }
    }
    FAIL("Binding (" << group << ", " << slot << ") fehlt");
    return gpu::TextureHandle{};
}

/// Der Parameterpuffer des letzten Dispatches.
inline std::vector<std::byte> parametersOf(const StageFixture& fixture) {
    return fixture.device.bufferBytes(
        bufferAt(fixture.lastDispatch(), compiler::kFieldBindGroup, compiler::kParametersBinding));
}

inline compiler::FrameUniforms frameOf(const StageFixture& fixture) {
    const std::vector<std::byte> bytes = fixture.device.bufferBytes(
        bufferAt(fixture.lastDispatch(), compiler::kFieldBindGroup, compiler::kFrameBinding));
    compiler::FrameUniforms frame;
    REQUIRE(bytes.size() == sizeof frame);
    std::memcpy(&frame, bytes.data(), sizeof frame);
    return frame;
}

}  // namespace whitehole::test
