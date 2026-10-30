// Befehlsaufzeichnung (submit), Dispatch mit Bind-Groups und Readback.
#include <algorithm>
#include <utility>

#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "webgpu_impl.hpp"

namespace whitehole::gpu {

namespace {

/// Zeichnet die Befehle eines Frames auf. Eine Compute-Pass-Phase bleibt offen, bis ein
/// Kopierbefehl kommt (Kopien dürfen nicht innerhalb eines Passes stehen).
class Recorder final : public ICommandRecorder {
public:
    Recorder(WebGpuDevice::Impl& impl, WGPUCommandEncoder encoder) : impl_{impl}, encoder_{encoder} {}

    ~Recorder() override {
        endPass();
        for (WGPUBindGroup group : bindGroups_) {
            wgpuBindGroupRelease(group);
        }
    }

    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    void closePass() { endPass(); }

    void dispatch(const DispatchDesc& desc) override {
        const auto pipelineIt = impl_.pipelines.find(desc.pipeline.value());
        if (pipelineIt == impl_.pipelines.end()) {
            fail("dispatch: unbekannte Pipeline");
            return;
        }
        const PipelineEntry& pipeline = pipelineIt->second;

        // Bind-Groups zuerst bauen: Schlägt etwas fehl, wird nichts aufgezeichnet.
        std::vector<std::pair<std::uint32_t, WGPUBindGroup>> groups;
        for (std::uint32_t group = 0; group < pipeline.layouts.size(); ++group) {
            WGPUBindGroup built = buildGroup(pipeline, group, desc.bindings);
            if (built == nullptr) {
                return;  // Fehler ist gesetzt
            }
            groups.emplace_back(group, built);
        }

        beginPass();
        wgpuComputePassEncoderSetPipeline(pass_, pipeline.pipeline);
        for (const auto& [index, group] : groups) {
            wgpuComputePassEncoderSetBindGroup(pass_, index, group, 0, nullptr);
        }
        wgpuComputePassEncoderDispatchWorkgroups(pass_, desc.groupsX, desc.groupsY, desc.groupsZ);
    }

    void copyTextureToBuffer(TextureHandle source, BufferHandle destination) override {
        const auto t = impl_.textures.find(source.value());
        const auto b = impl_.buffers.find(destination.value());
        if (t == impl_.textures.end() || b == impl_.buffers.end()) {
            fail("copyTextureToBuffer: unbekannte Ressource");
            return;
        }
        const TextureEntry& texture = t->second;
        const BufferEntry& buffer = b->second;
        if (!hasFlag(texture.usage, Usage::CopySource) || !hasFlag(buffer.usage, Usage::CopyDestination)) {
            fail("copyTextureToBuffer: Quelle braucht CopySource, Ziel CopyDestination");
            return;
        }
        const std::uint32_t rowBytes = paddedBytesPerRow(texture.extent.width, texture.format);
        const std::uint64_t needed = std::uint64_t{rowBytes} * texture.extent.height;
        if (buffer.size < needed) {
            fail("copyTextureToBuffer: Zielpuffer zu klein (" + std::to_string(buffer.size) + " statt " +
                 std::to_string(needed) + " Bytes bei auf 256 aufgefüllten Zeilen)");
            return;
        }
        endPass();
        WGPUTexelCopyTextureInfo from{};
        from.texture = texture.texture;
        from.aspect = WGPUTextureAspect_All;
        WGPUTexelCopyBufferInfo to{};
        to.buffer = buffer.buffer;
        to.layout.bytesPerRow = rowBytes;
        to.layout.rowsPerImage = texture.extent.height;
        const WGPUExtent3D size{texture.extent.width, texture.extent.height, 1};
        wgpuCommandEncoderCopyTextureToBuffer(encoder_, &from, &to, &size);
    }

    void copyBufferToBuffer(BufferHandle source, BufferHandle destination, std::uint64_t sizeBytes) override {
        const auto s = impl_.buffers.find(source.value());
        const auto d = impl_.buffers.find(destination.value());
        if (s == impl_.buffers.end() || d == impl_.buffers.end()) {
            fail("copyBufferToBuffer: unbekannter Puffer");
            return;
        }
        if (!hasFlag(s->second.usage, Usage::CopySource) || !hasFlag(d->second.usage, Usage::CopyDestination)) {
            fail("copyBufferToBuffer: Quelle braucht CopySource, Ziel CopyDestination");
            return;
        }
        if (sizeBytes % 4 != 0 || sizeBytes > s->second.size || sizeBytes > d->second.size) {
            fail("copyBufferToBuffer: Länge muss durch 4 teilbar sein und in beide Puffer passen");
            return;
        }
        endPass();
        wgpuCommandEncoderCopyBufferToBuffer(encoder_, s->second.buffer, 0, d->second.buffer, 0, sizeBytes);
    }

private:
    void fail(std::string message) {
        if (error_.empty()) {
            error_ = std::move(message);
        }
    }

    void beginPass() {
        if (pass_ == nullptr) {
            WGPUComputePassDescriptor pd{};
            pass_ = wgpuCommandEncoderBeginComputePass(encoder_, &pd);
        }
    }

    void endPass() {
        if (pass_ != nullptr) {
            wgpuComputePassEncoderEnd(pass_);
            wgpuComputePassEncoderRelease(pass_);
            pass_ = nullptr;
        }
    }

    /// Baut die Bind-Group einer Gruppe aus den übergebenen Bindungen. Alle deklarierten
    /// Bindungen der Gruppe müssen vorhanden und passend sein; überzählige werden abgelehnt.
    WGPUBindGroup buildGroup(const PipelineEntry& pipeline, std::uint32_t group, std::span<const Binding> bindings) {
        std::vector<WGPUBindGroupEntry> entries;
        std::size_t declaredInGroup = 0;
        for (const DeclaredBinding& decl : pipeline.bindings) {
            if (decl.group != group) {
                continue;
            }
            ++declaredInGroup;
            const Binding* match = nullptr;
            for (const Binding& b : bindings) {
                if (b.group == group && b.slot == decl.binding) {
                    match = &b;
                    break;
                }
            }
            if (match == nullptr) {
                fail("dispatch: @group(" + std::to_string(group) + ") @binding(" + std::to_string(decl.binding) +
                     ") " + decl.name + " ist nicht gebunden");
                return nullptr;
            }
            const BufferEntry* buffer = nullptr;
            const TextureEntry* texture = nullptr;
            if (const auto* h = std::get_if<BufferHandle>(&match->resource)) {
                const auto it = impl_.buffers.find(h->value());
                buffer = it != impl_.buffers.end() ? &it->second : nullptr;
            } else if (const auto* t = std::get_if<TextureHandle>(&match->resource)) {
                const auto it = impl_.textures.find(t->value());
                texture = it != impl_.textures.end() ? &it->second : nullptr;
            }
            if (std::string why = checkResource(decl, *match, buffer, texture); !why.empty()) {
                fail("dispatch: " + why);
                return nullptr;
            }
            WGPUBindGroupEntry entry{};
            entry.binding = decl.binding;
            if (buffer != nullptr) {
                entry.buffer = buffer->buffer;
                entry.offset = 0;
                entry.size = buffer->size;
            } else {
                entry.textureView = texture->view;
            }
            entries.push_back(entry);
        }
        const auto boundInGroup = static_cast<std::size_t>(std::count_if(
            bindings.begin(), bindings.end(), [group](const Binding& b) { return b.group == group; }));
        if (boundInGroup != declaredInGroup) {
            fail("dispatch: In @group(" + std::to_string(group) + ") sind Bindungen gesetzt, die der Shader nicht deklariert");
            return nullptr;
        }

        WGPUBindGroupDescriptor bgd{};
        bgd.layout = pipeline.layouts[group];
        bgd.entryCount = entries.size();
        bgd.entries = entries.data();
        WGPUBindGroup built = wgpuDeviceCreateBindGroup(impl_.device, &bgd);
        if (built == nullptr) {
            fail("dispatch: Bind-Group ließ sich nicht anlegen");
            return nullptr;
        }
        bindGroups_.push_back(built);
        return built;
    }

    WebGpuDevice::Impl& impl_;
    WGPUCommandEncoder encoder_;
    WGPUComputePassEncoder pass_{nullptr};
    std::vector<WGPUBindGroup> bindGroups_;
    std::string error_;
};

void onMapped(WGPUMapAsyncStatus status, WGPUStringView message, void* user, void*) {
    auto* state = static_cast<ReadbackState*>(user);
    state->status = status;
    state->message = toString(message);
    state->done = true;
}

}  // namespace

Result<void> WebGpuDevice::submit(const RecordFn& record) {
    Impl& impl = *impl_;
    impl.drainErrors();

    WGPUCommandEncoderDescriptor ed{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(impl.device, &ed);
    if (encoder == nullptr) {
        return Error{ErrorCode::GpuError, "Der Command-Encoder ließ sich nicht anlegen"};
    }

    std::string recorderError;
    WGPUCommandBuffer commands = nullptr;
    {
        Recorder recorder{impl, encoder};
        record(recorder);
        recorder.closePass();
        recorderError = recorder.error();
        if (recorderError.empty()) {
            commands = wgpuCommandEncoderFinish(encoder, nullptr);
        }
    }  // Recorder gibt Pass und Bind-Groups frei, bevor der Encoder verschwindet
    wgpuCommandEncoderRelease(encoder);

    if (!recorderError.empty()) {
        if (commands != nullptr) wgpuCommandBufferRelease(commands);
        return Error{ErrorCode::GpuError, recorderError};
    }
    if (commands == nullptr) {
        return Error{ErrorCode::GpuError, "Die Befehle ließen sich nicht abschließen"};
    }
    wgpuQueueSubmit(impl.queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    impl.pump(false);
    const std::vector<std::string> errors = impl.drainErrors();
    if (!errors.empty()) {
        std::string joined = errors.front();
        for (std::size_t i = 1; i < errors.size(); ++i) {
            joined += "; " + errors[i];
        }
        return Error{ErrorCode::GpuError, joined};
    }
    return {};
}

void WebGpuDevice::readbackAsync(BufferHandle buffer, ReadbackFn onComplete) {
    Impl& impl = *impl_;
    auto* state = new ReadbackState;
    state->callback = std::move(onComplete);
    state->bufferId = buffer.value();

    const auto it = impl.buffers.find(buffer.value());
    if (it == impl.buffers.end() || !hasFlag(it->second.usage, Usage::MapRead)) {
        // Der Fehler wird wie jedes andere Ergebnis erst in poll() ausgeliefert.
        state->status = WGPUMapAsyncStatus_Error;
        state->message = it == impl.buffers.end() ? "unbekannter Puffer" : "Puffer ohne Usage::MapRead";
        state->done = true;
        impl.readbacks.push_back(state);
        return;
    }

    state->buffer = it->second.buffer;
    wgpuBufferAddRef(state->buffer);
    state->size = it->second.size;
    ++it->second.pendingMaps;
    impl.readbacks.push_back(state);

    WGPUBufferMapCallbackInfo info{};
    info.mode = WGPUCallbackMode_AllowProcessEvents;
    info.callback = onMapped;
    info.userdata1 = state;
    wgpuBufferMapAsync(state->buffer, WGPUMapMode_Read, 0, static_cast<size_t>(state->size), info);
}

}  // namespace whitehole::gpu
