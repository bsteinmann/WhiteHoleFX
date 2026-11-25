// Pipelines im Hintergrund übersetzen auf einem echten Gerät (WebGpuDevice::beginComputePipeline): fertige Pipeline
// läuft, ungültiger Shader wird sofort gemeldet, verworfene Marken räumen auf. Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <optional>
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

ComputePipelineDesc pipelineOf(const std::string& code) {
    ComputePipelineDesc desc;
    desc.shader.code = code;
    return desc;
}

const char* const kFillShader = R"(
@group(0) @binding(0) var<storage, read_write> data: array<u32>;
@compute @workgroup_size(8)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    if (id.x < arrayLength(&data)) {
        data[id.x] = id.x * 5u + 2u;
    }
}
)";

/// Holt die Marke ab, höchstens 10 s lang.
std::optional<Result<ComputePipelineHandle>> waitFor(WebGpuDevice& device, PipelineTicket ticket) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::optional<Result<ComputePipelineHandle>> done = device.takeComputePipeline(ticket)) {
            return done;
        }
        device.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

}  // namespace

TEST_CASE("Hintergrund-Pipeline: wird fertig und rechnet", "[gpu_device][background]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<PipelineTicket> ticket = device->beginComputePipeline(pipelineOf(kFillShader));
    REQUIRE(ticket.ok());
    const std::optional<Result<ComputePipelineHandle>> pipeline = waitFor(*device, ticket.value());
    REQUIRE(pipeline.has_value());
    REQUIRE(pipeline->ok());

    BufferDesc dataDesc;
    dataDesc.sizeBytes = 64;
    dataDesc.usage = Usage::Storage | Usage::CopySource;
    const BufferHandle data = device->createBuffer(dataDesc).value();
    BufferDesc readDesc;
    readDesc.sizeBytes = 64;
    readDesc.usage = Usage::MapRead | Usage::CopyDestination;
    const BufferHandle target = device->createBuffer(readDesc).value();
    const std::vector<Binding> bindings{Binding{0, 0, data}};
    REQUIRE(device->submit([&](ICommandRecorder& r) {
                      r.dispatch(DispatchDesc{pipeline->value(), bindings, 2, 1, 1});
                      r.copyBufferToBuffer(data, target, 64);
                  }).ok());
    bool done = false;
    std::vector<std::uint32_t> got(16);
    device->readbackAsync(target, [&](Result<std::span<const std::byte>> r) {
        done = true;
        if (r.ok()) {
            std::memcpy(got.data(), r.value().data(), 64);
        }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        device->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(done);
    for (std::uint32_t i = 0; i < 16; ++i) {
        CHECK(got[i] == i * 5U + 2U);
    }
    // Eine verbrauchte Marke ist unbekannt
    const std::optional<Result<ComputePipelineHandle>> again = device->takeComputePipeline(ticket.value());
    REQUIRE(again.has_value());
    CHECK_FALSE(again->ok());
}

TEST_CASE("Hintergrund-Pipeline: ein ungültiger Shader wird sofort gemeldet", "[gpu_device][background]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<PipelineTicket> ticket = device->beginComputePipeline(pipelineOf(
        "@group(0) @binding(0) var<storage, read_write> data: array<u32>;\n"
        "@compute @workgroup_size(1) fn main() { data[0] = gibtsnicht; }\n"));
    REQUIRE_FALSE(ticket.ok());
    CHECK(ticket.error().code == ErrorCode::ShaderError);
}

TEST_CASE("Hintergrund-Pipeline: verworfene Marken und Abbau mitten im Übersetzen", "[gpu_device][background]") {
    std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    const Result<PipelineTicket> first = device->beginComputePipeline(pipelineOf(kFillShader));
    REQUIRE(first.ok());
    device->cancelComputePipeline(first.value());
    const std::optional<Result<ComputePipelineHandle>> gone = device->takeComputePipeline(first.value());
    REQUIRE(gone.has_value());
    CHECK_FALSE(gone->ok());
    for (int i = 0; i < 5; ++i) {
        device->poll();
    }
    // Eine noch laufende Übersetzung beim Abbau des Geräts: der Destruktor wartet und gibt frei
    REQUIRE(device->beginComputePipeline(pipelineOf(kFillShader)).ok());
    device.reset();
    SUCCEED();
}
