// WebGpuDevice auf einer echten GPU (oder einem Software-Adapter wie lavapipe).
// Ohne Adapter werden die Tests übersprungen, nicht als Fehler gewertet.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "whitehole/gpu_webgpu/webgpu_device.hpp"

using namespace whitehole;
using namespace whitehole::gpu;

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

/// Fragt den Puffer ab und wartet (höchstens 5 s) auf das Ergebnis.
Result<std::vector<std::byte>> readBack(WebGpuDevice& device, BufferHandle buffer) {
    bool done = false;
    Result<std::vector<std::byte>> result = Error{ErrorCode::Internal, "kein Ergebnis"};
    device.readbackAsync(buffer, [&](Result<std::span<const std::byte>> r) {
        done = true;
        result = r.ok() ? Result<std::vector<std::byte>>{std::vector<std::byte>(r.value().begin(), r.value().end())}
                        : Result<std::vector<std::byte>>{r.error()};
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        device.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return result;
}

BufferHandle makeBuffer(WebGpuDevice& device, std::uint64_t size, Usage usage) {
    BufferDesc desc;
    desc.sizeBytes = size;
    desc.usage = usage;
    Result<BufferHandle> r = device.createBuffer(desc);
    REQUIRE(r.ok());
    return r.value();
}

ComputePipelineDesc pipelineOf(const std::string& code) {
    ComputePipelineDesc desc;
    desc.shader.code = code;
    return desc;
}

const char* const kFillShader = R"(
@group(0) @binding(0) var<storage, read_write> data: array<u32>;
@group(0) @binding(1) var<uniform> unusedParams: vec4<u32>;
@compute @workgroup_size(8)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    if (id.x < arrayLength(&data)) {
        data[id.x] = id.x * 3u + 1u;
    }
}
)";

}  // namespace

TEST_CASE("Ein Gerät lässt sich anlegen", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    INFO("Adapter: " << device->adapterName());
    CHECK(device->takeErrors().empty());
}

TEST_CASE("Puffer: schreiben, kopieren, zurücklesen", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const BufferHandle source =
        makeBuffer(*device, 64, Usage::Storage | Usage::CopyDestination | Usage::CopySource);
    const BufferHandle target = makeBuffer(*device, 64, Usage::MapRead | Usage::CopyDestination);

    std::vector<float> values(16);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<float>(i) * 1.5F;
    }
    REQUIRE(device->writeBuffer(source, 0, std::as_bytes(std::span{values})).ok());
    REQUIRE(device->submit([&](ICommandRecorder& r) { r.copyBufferToBuffer(source, target, 64); }).ok());

    const Result<std::vector<std::byte>> back = readBack(*device, target);
    REQUIRE(back.ok());
    REQUIRE(back.value().size() == 64);
    std::vector<float> got(16);
    std::memcpy(got.data(), back.value().data(), 64);
    CHECK(got == values);
}

TEST_CASE("Compute-Shader läuft; eine ungenutzte Bindung gehört trotzdem zum Layout", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<ComputePipelineHandle> pipeline = device->createComputePipeline(pipelineOf(kFillShader));
    REQUIRE(pipeline.ok());

    const BufferHandle data = makeBuffer(*device, 128, Usage::Storage | Usage::CopySource);
    const BufferHandle params = makeBuffer(*device, 16, Usage::Uniform | Usage::CopyDestination);
    const BufferHandle target = makeBuffer(*device, 128, Usage::MapRead | Usage::CopyDestination);

    const std::vector<Binding> bindings{Binding{0, 0, data}, Binding{0, 1, params}};
    REQUIRE(device->submit([&](ICommandRecorder& r) {
                      r.dispatch(DispatchDesc{pipeline.value(), bindings, 4, 1, 1});
                      r.copyBufferToBuffer(data, target, 128);
                  }).ok());

    const Result<std::vector<std::byte>> back = readBack(*device, target);
    REQUIRE(back.ok());
    std::vector<std::uint32_t> got(32);
    std::memcpy(got.data(), back.value().data(), 128);
    for (std::uint32_t i = 0; i < 32; ++i) {
        REQUIRE(got[i] == i * 3U + 1U);
    }
}

TEST_CASE("Eine fehlende Bindung wird gemeldet und nichts läuft", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<ComputePipelineHandle> pipeline = device->createComputePipeline(pipelineOf(kFillShader));
    REQUIRE(pipeline.ok());
    const BufferHandle data = makeBuffer(*device, 128, Usage::Storage);

    const std::vector<Binding> onlyOne{Binding{0, 0, data}};
    const Result<void> r = device->submit(
        [&](ICommandRecorder& rec) { rec.dispatch(DispatchDesc{pipeline.value(), onlyOne, 1, 1, 1}); });
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().code == ErrorCode::GpuError);
    CHECK(r.error().message.find("nicht gebunden") != std::string::npos);
}

TEST_CASE("Ein Puffer mit falschem Usage wird abgelehnt", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<ComputePipelineHandle> pipeline = device->createComputePipeline(pipelineOf(kFillShader));
    REQUIRE(pipeline.ok());
    const BufferHandle notStorage = makeBuffer(*device, 128, Usage::CopyDestination);
    const BufferHandle params = makeBuffer(*device, 16, Usage::Uniform);
    const std::vector<Binding> bindings{Binding{0, 0, notStorage}, Binding{0, 1, params}};
    const Result<void> r = device->submit(
        [&](ICommandRecorder& rec) { rec.dispatch(DispatchDesc{pipeline.value(), bindings, 1, 1, 1}); });
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("Usage::Storage") != std::string::npos);
}

TEST_CASE("Kaputtes WGSL ergibt einen ShaderError", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<ComputePipelineHandle> bad =
        device->createComputePipeline(pipelineOf("@compute @workgroup_size(1) fn main( {"));
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::ShaderError);
    // Das Gerät bleibt benutzbar.
    CHECK(device->createComputePipeline(pipelineOf(kFillShader)).ok());
}

TEST_CASE("Textur -> Puffer: Zeilen sind auf 256 Byte aufgefüllt", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const char* const shader = R"(
@group(0) @binding(0) var tex: texture_storage_2d<rgba8unorm, write>;
@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let size = textureDimensions(tex);
    if (id.x >= size.x || id.y >= size.y) { return; }
    textureStore(tex, vec2<i32>(id.xy), vec4<f32>(f32(id.x) / 255.0, f32(id.y) / 255.0, 0.0, 1.0));
}
)";
    const Result<ComputePipelineHandle> pipeline = device->createComputePipeline(pipelineOf(shader));
    REQUIRE(pipeline.ok());

    TextureDesc td;
    td.extent = Extent3{10, 4, 1};
    td.format = TextureFormat::Rgba8Unorm;
    td.usage = Usage::Storage | Usage::CopySource;
    const Result<TextureHandle> texture = device->createTexture(td);
    REQUIRE(texture.ok());

    const std::uint32_t rowBytes = paddedBytesPerRow(10, TextureFormat::Rgba8Unorm);
    CHECK(rowBytes == 256);
    const BufferHandle target = makeBuffer(*device, std::uint64_t{rowBytes} * 4, Usage::MapRead | Usage::CopyDestination);

    const std::vector<Binding> bindings{Binding{0, 0, texture.value()}};
    REQUIRE(device->submit([&](ICommandRecorder& r) {
                      r.dispatch(DispatchDesc{pipeline.value(), bindings, 2, 1, 1});
                      r.copyTextureToBuffer(texture.value(), target);
                  }).ok());

    const Result<std::vector<std::byte>> back = readBack(*device, target);
    REQUIRE(back.ok());
    REQUIRE(back.value().size() == std::size_t{rowBytes} * 4);
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 10; ++x) {
            const std::byte* p = back.value().data() + std::size_t{y} * rowBytes + std::size_t{x} * 4;
            REQUIRE(std::to_integer<int>(p[0]) == static_cast<int>(x));
            REQUIRE(std::to_integer<int>(p[1]) == static_cast<int>(y));
            REQUIRE(std::to_integer<int>(p[2]) == 0);
            REQUIRE(std::to_integer<int>(p[3]) == 255);
        }
    }
}

TEST_CASE("Fehlerfälle bei Kopie und Readback", "[gpu_device]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    // Readback eines Puffers ohne MapRead: Fehler wird in poll() geliefert.
    const BufferHandle plain = makeBuffer(*device, 16, Usage::Storage);
    const Result<std::vector<std::byte>> back = readBack(*device, plain);
    REQUIRE_FALSE(back.ok());
    CHECK(back.error().code == ErrorCode::GpuError);

    // Kopie in einen zu kleinen Puffer.
    TextureDesc td;
    td.extent = Extent3{10, 4, 1};
    td.format = TextureFormat::Rgba8Unorm;
    td.usage = Usage::Storage | Usage::CopySource;
    const Result<TextureHandle> texture = device->createTexture(td);
    REQUIRE(texture.ok());
    const BufferHandle tooSmall = makeBuffer(*device, 40 * 4, Usage::MapRead | Usage::CopyDestination);
    const Result<void> r = device->submit([&](ICommandRecorder& rec) { rec.copyTextureToBuffer(texture.value(), tooSmall); });
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("zu klein") != std::string::npos);

    // Ungültige Schreibzugriffe.
    const BufferHandle buf = makeBuffer(*device, 16, Usage::Storage | Usage::CopyDestination);
    const std::vector<std::byte> odd(3);
    CHECK(device->writeBuffer(buf, 0, odd).error().code == ErrorCode::InvalidArgument);
    const std::vector<std::byte> big(32);
    CHECK(device->writeBuffer(buf, 0, big).error().code == ErrorCode::InvalidArgument);
    CHECK(device->writeBuffer(BufferHandle{9999}, 0, std::span<const std::byte>{}).error().code == ErrorCode::NotFound);
}
