// webgpu_device.hpp: IGpuDevice auf einer echten GPU (oder einem Software-Rasterizer).
//
// Warum es diese Datei gibt: Bis hierhin lief alles gegen ein Fake-Gerät. Diese Klasse
// führt die erzeugten WGSL-Shader wirklich aus. Sie setzt die Schnittstelle IGpuDevice
// über die WebGPU-C-Schnittstelle (webgpu.h) um, die Dawn und wgpu-native gemeinsam haben.
// Kein Header des Moduls enthält webgpu.h; der Rest der Engine sieht nur IGpuDevice.
//
// Aufgabe und Verhalten:
//   - Puffer und Texturen mit den Usage-Flags der Engine; Puffergrößen werden intern auf
//     ein Vielfaches von 4 aufgerundet (WebGPU verlangt das für Kopien und Mapping).
//   - Compute-Pipelines: Das Bind-Group-Layout wird aus den @group/@binding-Deklarationen
//     des WGSL-Textes gebaut (wgsl_bindings.hpp), nicht von WebGPU abgeleitet. Dadurch
//     gehört jede deklarierte Bindung zum Layout, auch wenn der Shader sie nicht benutzt.
//   - dispatch(): Es müssen alle deklarierten Bindungen der benutzten Gruppen gebunden
//     sein, mit passender Ressourcenart und passendem Usage. Sonst wird der Dispatch nicht
//     ausgeführt und submit() meldet einen Fehler.
//   - copyTextureToBuffer(): Zeilen im Zielpuffer sind auf 256 Byte aufgefüllt (Vorgabe
//     von WebGPU). Die Zeilenlänge liefert paddedBytesPerRow().
//   - readbackAsync(): Der Puffer braucht Usage::MapRead. Der Callback läuft in poll().
//   - Fehler der GPU kommen asynchron. Sie werden gesammelt; submit() und poll() melden
//     sie, takeErrors() liefert sie für Diagnose.
//
// Threads: Alle Aufrufe von einem Thread (dem Engine-Thread).
// Lebensdauer: Das Gerät muss alle Callbacks und Stages überleben, die es benutzen.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/gpu/gpu_device.hpp"

namespace whitehole::gpu {

struct WebGpuOptions {
    /// Bevorzugt einen Software-Adapter (z. B. lavapipe). Nützlich für Tests ohne GPU.
    bool forceFallbackAdapter{false};
    std::string label{"WhiteHoleFX"};
};

/// Zeilenlänge in Bytes, die copyTextureToBuffer im Zielpuffer verwendet:
/// width * Bytes je Texel, aufgerundet auf ein Vielfaches von 256.
[[nodiscard]] std::uint32_t paddedBytesPerRow(std::uint32_t width, TextureFormat format) noexcept;

/// Bytes je Texel des Formats.
[[nodiscard]] std::uint32_t bytesPerTexel(TextureFormat format) noexcept;

class WebGpuDevice final : public IGpuDevice {
public:
    /// Fehler: GpuError, wenn kein Adapter oder Gerät verfügbar ist.
    [[nodiscard]] static Result<std::unique_ptr<WebGpuDevice>> create(const WebGpuOptions& options = {});

    ~WebGpuDevice() override;
    WebGpuDevice(const WebGpuDevice&) = delete;
    WebGpuDevice& operator=(const WebGpuDevice&) = delete;

    [[nodiscard]] Result<TextureHandle> createTexture(const TextureDesc& desc) override;
    [[nodiscard]] Result<BufferHandle> createBuffer(const BufferDesc& desc) override;
    [[nodiscard]] Result<ComputePipelineHandle> createComputePipeline(const ComputePipelineDesc& desc) override;

    void destroy(TextureHandle handle) noexcept override;
    void destroy(BufferHandle handle) noexcept override;
    void destroy(ComputePipelineHandle handle) noexcept override;

    Result<void> writeBuffer(BufferHandle buffer, std::uint64_t offsetBytes,
                             std::span<const std::byte> data) override;
    Result<void> submit(const RecordFn& record) override;
    void readbackAsync(BufferHandle buffer, ReadbackFn onComplete) override;
    void poll() override;

    /// Fehlermeldungen der GPU, die seit dem letzten Aufruf eingegangen sind.
    [[nodiscard]] std::vector<std::string> takeErrors();

    /// Name des Adapters (z. B. "llvmpipe" oder die Grafikkarte), zur Anzeige und für Logs.
    [[nodiscard]] const std::string& adapterName() const noexcept;

    struct Impl;

private:
    explicit WebGpuDevice(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

}  // namespace whitehole::gpu
