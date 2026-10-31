// gpu_stage_support.hpp: Hilfen für die GPU-Tests der Canvas-Stage (Gerät anlegen, Textur zurücklesen).
//
// Warum es diese Datei gibt: Mehrere Testdateien führen vom Feldcompiler erzeugten WGSL-Code auf einem
// echten Gerät aus; die gemeinsamen Hilfen stehen hier, damit keine Datei sie kopiert.
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "whitehole/executor/canvas_render_stage.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"

namespace whitehole::gputest {

using namespace whitehole;
using namespace whitehole::gpu;


inline std::unique_ptr<WebGpuDevice> makeDevice() {
    Result<std::unique_ptr<WebGpuDevice>> device = WebGpuDevice::create();
    if (!device.ok()) {
        WebGpuOptions software;
        software.forceFallbackAdapter = true;
        device = WebGpuDevice::create(software);
    }
    return device.ok() ? std::move(device.value()) : nullptr;
}

inline std::vector<std::byte> readBack(WebGpuDevice& device, BufferHandle buffer) {
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

inline BufferHandle readbackBuffer(WebGpuDevice& device, std::uint64_t size) {
    BufferDesc desc;
    desc.sizeBytes = size;
    desc.usage = Usage::MapRead | Usage::CopyDestination;
    Result<BufferHandle> r = device.createBuffer(desc);
    REQUIRE(r.ok());
    return r.value();
}

inline FrameTime frameAt(std::uint64_t frame, double seconds) {
    FrameTime time;
    time.frame = frame;
    time.time = Seconds{seconds};
    return time;
}

/// Rendert die Canvas-Stage einmal und liest die Textur als Zeilen ohne Auffüllung zurück.
inline std::vector<std::byte> renderCanvas(WebGpuDevice& device, executor::CanvasRenderStage& stage, Extent3 extent,
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

}  // namespace whitehole::gputest
