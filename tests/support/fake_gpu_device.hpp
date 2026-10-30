// fake_gpu_device.hpp: In-Memory-GPU für Tests. Puffer sind einfache Byte-Vektoren,
// Readbacks werden erst bei poll() ausgeliefert und lassen sich anhalten.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "whitehole/gpu/gpu_device.hpp"

namespace whitehole::test {

class FakeGpuDevice final : public gpu::IGpuDevice {
public:
    Result<gpu::TextureHandle> createTexture(const gpu::TextureDesc& desc) override {
        const gpu::TextureHandle handle{++lastId_};
        textureDescs_.push_back(desc);
        liveTextures_.insert(handle.value());
        return handle;
    }

    Result<gpu::BufferHandle> createBuffer(const gpu::BufferDesc& desc) override {
        if (buffersUntilFailure_ == 0) {
            buffersUntilFailure_ = -1;
            return Error{ErrorCode::GpuError, "Puffer nicht angelegt"};
        }
        if (buffersUntilFailure_ > 0) {
            --buffersUntilFailure_;
        }
        const gpu::BufferHandle handle{++lastId_};
        buffers_[handle.value()].assign(static_cast<std::size_t>(desc.sizeBytes), std::byte{0});
        return handle;
    }

    Result<gpu::ComputePipelineHandle> createComputePipeline(
        const gpu::ComputePipelineDesc& desc) override {
        if (failNextPipeline_) {
            failNextPipeline_ = false;
            return Error{ErrorCode::ShaderError, "Pipeline fehlgeschlagen"};
        }
        const gpu::ComputePipelineHandle handle{++lastId_};
        pipelineDescs_.push_back(desc);
        livePipelines_.insert(handle.value());
        return handle;
    }

    void destroy(gpu::TextureHandle handle) noexcept override { liveTextures_.erase(handle.value()); }
    void destroy(gpu::BufferHandle handle) noexcept override { buffers_.erase(handle.value()); }
    void destroy(gpu::ComputePipelineHandle handle) noexcept override {
        livePipelines_.erase(handle.value());
    }

    Result<void> writeBuffer(gpu::BufferHandle buffer, std::uint64_t offsetBytes,
                             std::span<const std::byte> data) override {
        if (writesUntilFailure_ == 0) {
            writesUntilFailure_ = -1;
            return Error{ErrorCode::GpuError, "Schreiben fehlgeschlagen"};
        }
        if (writesUntilFailure_ > 0) {
            --writesUntilFailure_;
        }
        const auto it = buffers_.find(buffer.value());
        if (it == buffers_.end()) {
            return Error{ErrorCode::NotFound, "Puffer unbekannt"};
        }
        if (offsetBytes + static_cast<std::uint64_t>(data.size()) > it->second.size()) {
            return Error{ErrorCode::InvalidArgument, "Schreibzugriff außerhalb des Puffers"};
        }
        std::copy(data.begin(), data.end(),
                  it->second.begin() + static_cast<std::ptrdiff_t>(offsetBytes));
        return {};
    }

    Result<void> submit(const RecordFn& record) override {
        if (failNextSubmit_) {
            failNextSubmit_ = false;
            return Error{ErrorCode::GpuError, "submit fehlgeschlagen"};
        }
        Recorder recorder{*this};
        record(recorder);
        ++submitCount_;
        return {};
    }

    void readbackAsync(gpu::BufferHandle buffer, ReadbackFn onComplete) override {
        pending_.push_back(Pending{buffer, std::move(onComplete)});
    }

    void poll() override {
        ++pollCount_;
        if (holdReadbacks_) {
            return;
        }
        std::vector<Pending> ready;
        ready.swap(pending_);
        for (auto& entry : ready) {
            deliver(entry);
        }
    }

    // --- Testhilfen ---

    /// Solange gehalten wird, bleiben Readbacks in der Warteschlange.
    void holdReadbacks(bool hold) noexcept { holdReadbacks_ = hold; }
    void failNextSubmit() noexcept { failNextSubmit_ = true; }
    void failNextPipeline() noexcept { failNextPipeline_ = true; }
    void failNextBuffer() noexcept { buffersUntilFailure_ = 0; }
    /// Lässt createBuffer nach `skip` erfolgreichen Aufrufen einmal scheitern.
    void failBufferAfter(int skip) noexcept { buffersUntilFailure_ = skip; }
    /// Lässt writeBuffer nach `skip` erfolgreichen Aufrufen einmal scheitern.
    void failWriteAfter(int skip) noexcept { writesUntilFailure_ = skip; }

    /// Aufgezeichnete Dispatches (über alle Frames, in Reihenfolge).
    struct DispatchRecord {
        gpu::ComputePipelineHandle pipeline;
        std::vector<gpu::Binding> bindings;
        std::uint32_t groupsX{0};
        std::uint32_t groupsY{0};
        std::uint32_t groupsZ{0};
    };
    [[nodiscard]] const std::vector<DispatchRecord>& dispatches() const noexcept { return dispatches_; }
    [[nodiscard]] const std::vector<gpu::ComputePipelineDesc>& pipelineDescs() const noexcept {
        return pipelineDescs_;
    }
    [[nodiscard]] const std::vector<gpu::TextureDesc>& textureDescs() const noexcept {
        return textureDescs_;
    }
    [[nodiscard]] std::size_t livePipelineCount() const noexcept { return livePipelines_.size(); }
    [[nodiscard]] std::size_t liveTextureCount() const noexcept { return liveTextures_.size(); }
    [[nodiscard]] bool isPipelineAlive(gpu::ComputePipelineHandle handle) const {
        return livePipelines_.contains(handle.value());
    }
    /// Inhalt eines Puffers (leer, wenn er nicht existiert).
    [[nodiscard]] std::vector<std::byte> bufferBytes(gpu::BufferHandle handle) const {
        const auto it = buffers_.find(handle.value());
        return it == buffers_.end() ? std::vector<std::byte>{} : it->second;
    }

    void fill(gpu::BufferHandle buffer, std::byte value) {
        auto& data = buffers_.at(buffer.value());
        std::fill(data.begin(), data.end(), value);
    }

    [[nodiscard]] std::size_t liveBufferCount() const noexcept { return buffers_.size(); }
    [[nodiscard]] std::size_t pendingReadbacks() const noexcept { return pending_.size(); }
    [[nodiscard]] std::uint64_t submitCount() const noexcept { return submitCount_; }
    [[nodiscard]] std::uint64_t pollCount() const noexcept { return pollCount_; }

private:
    struct Pending {
        gpu::BufferHandle buffer;
        ReadbackFn callback;
    };

    class Recorder final : public gpu::ICommandRecorder {
    public:
        explicit Recorder(FakeGpuDevice& device) : device_{device} {}

        void dispatch(const gpu::DispatchDesc& desc) override {
            device_.dispatches_.push_back(
                DispatchRecord{desc.pipeline,
                               std::vector<gpu::Binding>(desc.bindings.begin(), desc.bindings.end()),
                               desc.groupsX, desc.groupsY, desc.groupsZ});
        }
        void copyTextureToBuffer(gpu::TextureHandle /*source*/,
                                 gpu::BufferHandle /*destination*/) override {}

        void copyBufferToBuffer(gpu::BufferHandle source, gpu::BufferHandle destination,
                                std::uint64_t sizeBytes) override {
            const auto from = device_.buffers_.find(source.value());
            const auto to = device_.buffers_.find(destination.value());
            if (from == device_.buffers_.end() || to == device_.buffers_.end()) {
                return;
            }
            const auto count = static_cast<std::ptrdiff_t>(
                std::min({sizeBytes, static_cast<std::uint64_t>(from->second.size()),
                          static_cast<std::uint64_t>(to->second.size())}));
            std::copy(from->second.begin(), from->second.begin() + count, to->second.begin());
        }

    private:
        FakeGpuDevice& device_;
    };

    void deliver(Pending& entry) {
        const auto it = buffers_.find(entry.buffer.value());
        if (it == buffers_.end()) {
            entry.callback(Error{ErrorCode::NotFound, "Puffer wurde zerstört"});
            return;
        }
        entry.callback(Result<std::span<const std::byte>>{std::span<const std::byte>{it->second}});
    }

    std::uint64_t lastId_{0};
    std::map<std::uint64_t, std::vector<std::byte>> buffers_;
    std::vector<Pending> pending_;
    bool holdReadbacks_{false};
    bool failNextSubmit_{false};
    bool failNextPipeline_{false};
    int buffersUntilFailure_{-1};
    int writesUntilFailure_{-1};
    std::vector<DispatchRecord> dispatches_;
    std::vector<gpu::ComputePipelineDesc> pipelineDescs_;
    std::vector<gpu::TextureDesc> textureDescs_;
    std::set<std::uint64_t> livePipelines_;
    std::set<std::uint64_t> liveTextures_;
    std::uint64_t submitCount_{0};
    std::uint64_t pollCount_{0};
};

}  // namespace whitehole::test
