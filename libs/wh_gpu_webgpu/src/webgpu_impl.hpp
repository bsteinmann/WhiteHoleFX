// webgpu_impl.hpp (intern): Zustand des WebGpuDevice.
//
// Warum es diese Datei gibt: Die Implementierung ist auf mehrere Quelldateien verteilt
// (Gerät, Ressourcen, Befehle), die denselben Zustand brauchen. Er liegt hier, damit
// kein öffentlicher Header Typen aus webgpu.h zeigt.
#pragma once

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>  // wgpuDevicePoll (Erweiterung von wgpu-native)

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "whitehole/gpu/gpu_device.hpp"
#include "whitehole/gpu/wgsl_bindings.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"

namespace whitehole::gpu {

struct BufferEntry {
    WGPUBuffer buffer{nullptr};
    std::uint64_t size{0};  // tatsächliche (aufgerundete) Größe
    Usage usage{Usage::None};
    std::uint32_t pendingMaps{0};
};

struct TextureEntry {
    WGPUTexture texture{nullptr};
    WGPUTextureView view{nullptr};
    Extent3 extent;
    TextureFormat format{TextureFormat::Rgba8Unorm};
    Usage usage{Usage::None};
};

struct PipelineEntry {
    WGPUComputePipeline pipeline{nullptr};
    std::vector<WGPUBindGroupLayout> layouts;  // Index = Gruppe
    std::vector<DeclaredBinding> bindings;     // sortiert
};

/// Ein abgeschlossenes Readback, das auf die Auslieferung in poll() wartet.
struct ReadbackState {
    WGPUBuffer buffer{nullptr};
    std::uint64_t size{0};
    std::uint64_t bufferId{0};
    IGpuDevice::ReadbackFn callback;
    WGPUMapAsyncStatus status{WGPUMapAsyncStatus_Error};
    std::string message;
    bool done{false};
};

struct WebGpuDevice::Impl {
    WGPUInstance instance{nullptr};
    WGPUAdapter adapter{nullptr};
    WGPUDevice device{nullptr};
    WGPUQueue queue{nullptr};
    std::string adapterName;

    std::uint64_t nextId{0};
    std::unordered_map<std::uint64_t, BufferEntry> buffers;
    std::unordered_map<std::uint64_t, TextureEntry> textures;
    std::unordered_map<std::uint64_t, PipelineEntry> pipelines;
    std::vector<ReadbackState*> readbacks;  // nicht abgeschlossen oder nicht ausgeliefert

    // Von der Fehler-Callback-Funktion beschrieben, vom Gerät gelesen.
    std::mutex errorMutex;
    std::vector<std::string> errors;

    ~Impl();
    void pushError(std::string message);
    std::vector<std::string> drainErrors();
    /// Lässt die Bibliothek wartende Arbeit abarbeiten (Callbacks, Fehlermeldungen).
    void pump(bool wait);
};

// webgpu_convert.cpp
[[nodiscard]] WGPUStringView view(const std::string& s) noexcept;
[[nodiscard]] std::string toString(WGPUStringView s);
[[nodiscard]] WGPUTextureFormat toWgpu(TextureFormat format) noexcept;
[[nodiscard]] WGPUTextureUsage toWgpuTexture(Usage usage) noexcept;
[[nodiscard]] WGPUBufferUsage toWgpuBuffer(Usage usage) noexcept;
[[nodiscard]] std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) noexcept;
/// Passt die Ressource zur deklarierten Bindung (Art und Usage)? Leer = ja, sonst der Grund.
[[nodiscard]] std::string checkResource(const DeclaredBinding& decl, const Binding& binding,
                                        const BufferEntry* buffer, const TextureEntry* texture);

}  // namespace whitehole::gpu
