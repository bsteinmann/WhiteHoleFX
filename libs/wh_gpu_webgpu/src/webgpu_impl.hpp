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
#include <optional>
#include <string>
#include <string_view>
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

/// GPU-Zeitmessung je submit (nur mit WebGpuOptions::timestamps und wenn der Adapter es kann, siehe
/// webgpu_timestamps.cpp): Anfang des ersten und Ende des letzten Compute-Passes eines Frames.
struct TimestampState {
    WGPUQuerySet querySet{nullptr};
    WGPUBuffer resolve{nullptr};  // QueryResolve | CopySrc, 2 × u64
    WGPUBuffer read{nullptr};     // MapRead | CopyDst
    double nsPerTick{1.0};
    bool recording{false};        // der laufende submit schreibt Zeitstempel
    bool firstPass{true};
    bool wrote{false};            // mindestens ein Pass hat geschrieben
    bool resolved{false};
    bool mapPending{false};
    bool mapDone{false};
    WGPUMapAsyncStatus mapStatus{WGPUMapAsyncStatus_Error};
    std::optional<double> lastMs;
};

struct WebGpuDevice::Impl {
    WGPUInstance instance{nullptr};
    WGPUAdapter adapter{nullptr};
    WGPUDevice device{nullptr};
    WGPUQueue queue{nullptr};
    std::string adapterName;
    std::string deviceName;  // Name ohne Zusatz "[Vulkan]"
    WGPUBackendType backendType{WGPUBackendType_Undefined};
    bool softwareAdapter{false};  // CPU-Adapter (WARP, lavapipe, llvmpipe) laut Adaptertyp oder Name
    std::string driver;  // Treiberbeschreibung des Adapters (WGPUAdapterInfo::description), leer wenn unbekannt
    TimestampState timestamps;

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

/// Kurzname der Grafikschnittstelle ("Vulkan", "OpenGL", "DX12", "Metal"), leer wenn unbekannt.
std::string_view backendLabel(WGPUBackendType type) noexcept;

// Zeitstempel (webgpu_timestamps.cpp). Alle Funktionen tun nichts, solange setupTimestamps keinen Query-Set angelegt hat.
/// Nach dem Anlegen des Geräts: Query-Set und Puffer anlegen. false, wenn das nicht ging (dann ohne Zeitstempel).
bool setupTimestamps(WebGpuDevice::Impl& impl);
void releaseTimestamps(WebGpuDevice::Impl& impl) noexcept;
/// Vor dem Aufzeichnen eines submit: misst dieser Frame mit (nicht, solange der vorige Wert noch gemappt wird)?
void beginFrameTimestamps(WebGpuDevice::Impl& impl);
/// Für jeden Compute-Pass: füllt `writes` und liefert den Zeiger für WGPUComputePassDescriptor, sonst nullptr.
const WGPUPassTimestampWrites* passTimestampWrites(WebGpuDevice::Impl& impl, WGPUPassTimestampWrites& writes);
/// Nach dem letzten Pass, vor wgpuCommandEncoderFinish: Werte in den Lese-Puffer kopieren.
void resolveFrameTimestamps(WebGpuDevice::Impl& impl, WGPUCommandEncoder encoder);
/// Nach wgpuQueueSubmit: Lese-Puffer mappen.
void afterSubmitTimestamps(WebGpuDevice::Impl& impl);
/// In poll(): gemappten Wert lesen.
void pollTimestamps(WebGpuDevice::Impl& impl);

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
