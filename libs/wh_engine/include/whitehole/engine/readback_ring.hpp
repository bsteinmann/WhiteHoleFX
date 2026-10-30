// readback_ring.hpp: Ring aus CPU-lesbaren Puffern für das asynchrone Zurücklesen.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/services.hpp"
#include "whitehole/engine/frame_sink.hpp"
#include "whitehole/gpu/gpu_device.hpp"

namespace whitehole::engine {

/// Verwaltet N Readback-Puffer. Ablauf pro Frame: acquire() -> GPU kopiert das
/// Ergebnis in den Slot -> commit() startet das asynchrone Zurücklesen. Sobald
/// die GPU fertig ist (im poll() des Geräts), geht der Frame an die Sink und
/// der Slot wird wieder frei. Ist kein Slot frei, liefert acquire() nichts;
/// die Engine verwirft dann den Frame, statt zu warten.
///
/// Threading: Alle Methoden laufen auf dem Engine-Thread, ebenso die
/// Callbacks (sie kommen aus IGpuDevice::poll()). Daher braucht der Ring keine
/// Mutexe. Das Gerät muss den Ring überleben.
class ReadbackRing {
public:
    struct Slot {
        std::size_t index{0};
        gpu::BufferHandle buffer;
    };

    /// Fehler: InvalidArgument bei 0 Slots oder 0 Bytes, sonst Fehler des Geräts.
    [[nodiscard]] static Result<std::unique_ptr<ReadbackRing>> create(
        gpu::IGpuDevice& device, IFrameSink& sink, std::size_t slotCount,
        std::uint64_t slotSizeBytes);

    ~ReadbackRing();
    ReadbackRing(const ReadbackRing&) = delete;
    ReadbackRing& operator=(const ReadbackRing&) = delete;

    /// Reserviert den nächsten freien Slot (reihum) oder liefert std::nullopt.
    [[nodiscard]] std::optional<Slot> acquire();

    /// Startet das Zurücklesen. Der Slot bleibt belegt, bis der Callback lief.
    void commit(const Slot& slot, FrameIndex frame);

    /// Gibt einen Slot zurück, ohne ein Readback zu starten (z. B. nach einem
    /// fehlgeschlagenen Einreichen).
    void release(const Slot& slot);

    [[nodiscard]] std::size_t freeSlots() const;

    /// Anzahl der Readbacks, die mit einem Fehler endeten (kein Frame geliefert).
    [[nodiscard]] std::uint64_t failedReadbacks() const;

private:
    struct State;

    ReadbackRing(gpu::IGpuDevice& device, std::shared_ptr<State> state,
                 std::vector<gpu::BufferHandle> buffers);

    gpu::IGpuDevice& device_;
    std::shared_ptr<State> state_;
    std::vector<gpu::BufferHandle> buffers_;
    std::size_t cursor_{0};
};

}  // namespace whitehole::engine
