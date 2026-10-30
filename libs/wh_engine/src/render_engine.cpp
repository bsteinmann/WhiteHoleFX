#include "whitehole/engine/render_engine.hpp"

#include <optional>
#include <utility>

namespace whitehole::engine {

RenderEngine::RenderEngine(gpu::IGpuDevice& device, IFrameClock& clock, ReadbackRing& ring,
                           RenderEngineConfig config)
    : device_{device}, clock_{clock}, ring_{ring}, config_{config} {}

void RenderEngine::setStages(std::vector<std::shared_ptr<IRenderStage>> stages) {
    stages_ = std::move(stages);
}

Result<void> RenderEngine::renderFrame() {
    const FrameTime time = clock_.advance();

    // Zuerst fertige Readbacks ausliefern: Das gibt Slots frei, bevor wir einen brauchen.
    device_.poll();

    const std::optional<ReadbackRing::Slot> slot = ring_.acquire();
    if (!slot.has_value()) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return {};
    }

    const Result<void> submitted = device_.submit([&](gpu::ICommandRecorder& recorder) {
        for (const auto& stage : stages_) {
            stage->record(time, recorder);
        }
        recorder.copyBufferToBuffer(config_.outputBuffer, slot->buffer, config_.outputSizeBytes);
    });
    if (!submitted.ok()) {
        ring_.release(*slot);
        return submitted;
    }

    ring_.commit(*slot, time.frame);
    rendered_.fetch_add(1, std::memory_order_relaxed);
    return {};
}

EngineStats RenderEngine::stats() const noexcept {
    return EngineStats{rendered_.load(std::memory_order_relaxed),
                       dropped_.load(std::memory_order_relaxed)};
}

}  // namespace whitehole::engine
