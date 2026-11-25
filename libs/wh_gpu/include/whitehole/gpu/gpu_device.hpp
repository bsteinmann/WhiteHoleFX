// gpu_device.hpp: Schnittstelle zum GPU-Backend (Dawn/WebGPU folgt in Schritt 3).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>

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

    /// Wie createComputePipeline, aber das teure Übersetzen durch den Treiber (bis zu Hunderte Millisekunden) darf im
    /// Hintergrund laufen, während weiter Frames entstehen. Fehler im Shadertext meldet schon dieser Aufruf; das Ergebnis
    /// holt takeComputePipeline ab. Standard (Geräte ohne Hintergrund): legt sofort an, takeComputePipeline ist dann
    /// gleich fertig. Fehler: die von createComputePipeline.
    [[nodiscard]] virtual Result<PipelineTicket> beginComputePipeline(const ComputePipelineDesc& desc) {
        Result<ComputePipelineHandle> made = createComputePipeline(desc);
        if (!made.ok()) {
            return made.error();
        }
        const PipelineTicket ticket{++nextTicket_};
        readyTickets_.emplace(ticket.value(), made.value());
        return ticket;
    }
    /// Ergebnis einer Marke: std::nullopt, solange der Treiber noch übersetzt; sonst die Pipeline oder der Fehler. Danach
    /// ist die Marke verbraucht. Fehler: ShaderError, NotFound (unbekannte Marke).
    [[nodiscard]] virtual std::optional<Result<ComputePipelineHandle>> takeComputePipeline(PipelineTicket ticket) {
        const auto it = readyTickets_.find(ticket.value());
        if (it == readyTickets_.end()) {
            return Result<ComputePipelineHandle>{Error{ErrorCode::NotFound, "Unbekannte Pipeline-Marke"}};
        }
        const ComputePipelineHandle handle = it->second;
        readyTickets_.erase(it);
        return Result<ComputePipelineHandle>{handle};
    }
    /// Verwirft eine Marke; eine fertige oder noch entstehende Pipeline wird freigegeben.
    virtual void cancelComputePipeline(PipelineTicket ticket) noexcept {
        if (const auto it = readyTickets_.find(ticket.value()); it != readyTickets_.end()) {
            destroy(it->second);
            readyTickets_.erase(it);
        }
    }

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

    /// GPU-Laufzeit des zuletzt fertig gemessenen submit in Millisekunden; danach vergessen. nullopt, wenn das Gerät
    /// keine Zeitstempel misst (Standard, siehe WebGpuOptions::timestamps) oder noch kein Wert da ist. Für den Benchmark.
    [[nodiscard]] virtual std::optional<double> takeGpuMilliseconds() { return std::nullopt; }

private:
    std::uint64_t nextTicket_{0};
    std::unordered_map<std::uint64_t, ComputePipelineHandle> readyTickets_;  // Standardweg: sofort fertig
};

}  // namespace whitehole::gpu
