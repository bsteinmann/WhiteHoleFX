// render_engine.hpp: setzt einen Frame von der Uhr bis zur Sink zusammen.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/services.hpp"
#include "whitehole/engine/readback_ring.hpp"
#include "whitehole/engine/render_stage.hpp"
#include "whitehole/gpu/gpu_device.hpp"

namespace whitehole::engine {

struct RenderEngineConfig {
    /// Puffer mit dem fertigen Ausgabe-Frame (Ergebnis der letzten Stage).
    gpu::BufferHandle outputBuffer;
    std::uint64_t outputSizeBytes{0};
};

struct EngineStats {
    std::uint64_t framesRendered{0};
    std::uint64_t framesDropped{0};  // verworfen, weil kein Readback-Slot frei war
};

/// Rahmen des Renderloops. Er kennt weder Graph noch Mapping; was gerechnet
/// wird, steckt in den Stages.
///
/// Threading: renderFrame() und setStages() laufen auf dem Engine-Thread.
/// stats() darf von jedem Thread gelesen werden.
/// Gerät, Uhr und Ring müssen die Engine überleben.
class RenderEngine {
public:
    RenderEngine(gpu::IGpuDevice& device, IFrameClock& clock, ReadbackRing& ring,
                 RenderEngineConfig config);

    void setStages(std::vector<std::shared_ptr<IRenderStage>> stages);

    /// Ein Frame: Uhr fortschreiben -> poll() (fertige Readbacks ausliefern) ->
    /// Slot holen -> Stages aufzeichnen -> Ausgabe in den Slot kopieren ->
    /// einreichen -> Readback starten.
    /// Ist kein Slot frei, wird der Frame verworfen (kein Fehler); die Uhr
    /// läuft trotzdem weiter, damit Animationen springen statt stehen zu bleiben.
    /// Fehler: nur wenn das Einreichen an die GPU scheitert.
    Result<void> renderFrame();

    [[nodiscard]] EngineStats stats() const noexcept;

private:
    gpu::IGpuDevice& device_;
    IFrameClock& clock_;
    ReadbackRing& ring_;
    RenderEngineConfig config_;
    std::vector<std::shared_ptr<IRenderStage>> stages_;
    std::atomic<std::uint64_t> rendered_{0};
    std::atomic<std::uint64_t> dropped_{0};
};

}  // namespace whitehole::engine
