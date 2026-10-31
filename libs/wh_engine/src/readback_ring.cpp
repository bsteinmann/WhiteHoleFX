// readback_ring.cpp: Umsetzung zu readback_ring.hpp. Ring aus CPU-lesbaren Puffern für das asynchrone Zurücklesen.
#include "whitehole/engine/readback_ring.hpp"

#include <span>
#include <string>
#include <utility>

namespace whitehole::engine {

/// Gemeinsamer Zustand, den auch die Readback-Callbacks besitzen. Dadurch ist
/// ein Callback, der nach dem Zerstören des Rings eintrifft, harmlos: `sink`
/// ist dann nullptr und der Frame wird verworfen.
struct ReadbackRing::State {
    IFrameSink* sink{nullptr};
    std::vector<bool> busy;
    std::uint64_t failed{0};
};

Result<std::unique_ptr<ReadbackRing>> ReadbackRing::create(gpu::IGpuDevice& device,
                                                           IFrameSink& sink,
                                                           std::size_t slotCount,
                                                           std::uint64_t slotSizeBytes) {
    if (slotCount == 0 || slotSizeBytes == 0) {
        return Error{ErrorCode::InvalidArgument, "Readback-Ring braucht mindestens einen Slot mit Größe > 0"};
    }

    std::vector<gpu::BufferHandle> buffers;
    buffers.reserve(slotCount);
    for (std::size_t i = 0; i < slotCount; ++i) {
        auto buffer = device.createBuffer(gpu::BufferDesc{
            .sizeBytes = slotSizeBytes,
            .usage = gpu::Usage::MapRead | gpu::Usage::CopyDestination,
            .label = "wh.readback.slot" + std::to_string(i)});
        if (!buffer.ok()) {
            for (const gpu::BufferHandle created : buffers) {
                device.destroy(created);
            }
            return buffer.error();
        }
        buffers.push_back(buffer.value());
    }

    auto state = std::make_shared<State>();
    state->sink = &sink;
    state->busy.assign(slotCount, false);

    return std::unique_ptr<ReadbackRing>(
        new ReadbackRing(device, std::move(state), std::move(buffers)));
}

ReadbackRing::ReadbackRing(gpu::IGpuDevice& device, std::shared_ptr<State> state,
                           std::vector<gpu::BufferHandle> buffers)
    : device_{device}, state_{std::move(state)}, buffers_{std::move(buffers)} {}

ReadbackRing::~ReadbackRing() {
    state_->sink = nullptr;  // späte Callbacks dürfen nichts mehr ausliefern
    for (const gpu::BufferHandle buffer : buffers_) {
        device_.destroy(buffer);
    }
}

std::optional<ReadbackRing::Slot> ReadbackRing::acquire() {
    for (std::size_t offset = 0; offset < buffers_.size(); ++offset) {
        const std::size_t index = (cursor_ + offset) % buffers_.size();
        if (!state_->busy[index]) {
            state_->busy[index] = true;
            cursor_ = (index + 1) % buffers_.size();
            return Slot{index, buffers_[index]};
        }
    }
    return std::nullopt;
}

void ReadbackRing::commit(const Slot& slot, FrameIndex frame) {
    device_.readbackAsync(
        slot.buffer,
        [state = state_, index = slot.index, frame](Result<std::span<const std::byte>> data) {
            state->busy[index] = false;
            if (!data.ok()) {
                ++state->failed;
                return;
            }
            if (state->sink == nullptr) {
                return;
            }
            const std::span<const std::byte> bytes = data.value();
            PixelFrame out;
            out.frame = frame;
            out.bytes.assign(bytes.begin(), bytes.end());
            state->sink->publish(std::move(out));
        });
}

void ReadbackRing::release(const Slot& slot) {
    state_->busy[slot.index] = false;
}

std::size_t ReadbackRing::freeSlots() const {
    std::size_t count = 0;
    for (const bool busy : state_->busy) {
        if (!busy) {
            ++count;
        }
    }
    return count;
}

std::uint64_t ReadbackRing::failedReadbacks() const {
    return state_->failed;
}

}  // namespace whitehole::engine
