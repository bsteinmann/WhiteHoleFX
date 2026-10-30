// gpu_device.hpp: Schnittstelle zum GPU-Backend (Dawn/WebGPU folgt in Schritt 3).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

#include "whitehole/core/result.hpp"
#include "whitehole/gpu/gpu_types.hpp"

namespace whitehole::gpu {

/// Nimmt die Befehle eines Frames auf. Wird nur innerhalb von
/// IGpuDevice::submit verwendet und darf nicht gespeichert werden.
class ICommandRecorder {
public:
    virtual ~ICommandRecorder() = default;

    virtual void dispatch(const DispatchDesc& desc) = 0;
    virtual void copyTextureToBuffer(TextureHandle source, BufferHandle destination) = 0;
    virtual void copyBufferToBuffer(BufferHandle source, BufferHandle destination,
                                    std::uint64_t sizeBytes) = 0;
};

class IGpuDevice {
public:
    using RecordFn = std::function<void(ICommandRecorder&)>;
    using ReadbackFn = std::function<void(Result<std::span<const std::byte>>)>;

    virtual ~IGpuDevice() = default;

    [[nodiscard]] virtual Result<TextureHandle> createTexture(const TextureDesc& desc) = 0;
    [[nodiscard]] virtual Result<BufferHandle> createBuffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual Result<ComputePipelineHandle> createComputePipeline(
        const ComputePipelineDesc& desc) = 0;

    virtual void destroy(TextureHandle handle) noexcept = 0;
    virtual void destroy(BufferHandle handle) noexcept = 0;
    virtual void destroy(ComputePipelineHandle handle) noexcept = 0;

    virtual Result<void> writeBuffer(BufferHandle buffer, std::uint64_t offsetBytes,
                                     std::span<const std::byte> data) = 0;

    /// Zeichnet alle Befehle eines Frames auf und reicht sie gesammelt ein.
    virtual Result<void> submit(const RecordFn& record) = 0;

    /// Liest einen Puffer asynchron zurück. Der Callback läuft, sobald die GPU
    /// fertig ist; der Span ist nur innerhalb des Callbacks gültig.
    virtual void readbackAsync(BufferHandle buffer, ReadbackFn onComplete) = 0;

    /// Verarbeitet abgeschlossene GPU-Arbeit und ruft fällige Callbacks auf.
    /// Callbacks laufen ausschließlich im Aufruf von poll(), also auf dem
    /// Thread, der poll() aufruft (der Engine-Thread).
    virtual void poll() = 0;
};

}  // namespace whitehole::gpu
