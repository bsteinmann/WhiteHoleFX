// webgpu_resources.cpp: Puffer, Texturen und Shadermodule des WebGPU-Geräts anlegen, beschreiben und freigeben.
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <utility>

#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "webgpu_impl.hpp"

namespace whitehole::gpu {

namespace {

Error gpuError(std::string message) {
    return Error{ErrorCode::GpuError, std::move(message)};
}

/// Hängt neu eingegangene GPU-Fehler an eine Meldung an.
std::string withErrors(std::string message, const std::vector<std::string>& errors) {
    for (const std::string& e : errors) {
        message += "; " + e;
    }
    return message;
}

WGPUBindGroupLayoutEntry layoutEntry(const DeclaredBinding& decl) {
    WGPUBindGroupLayoutEntry entry{};
    entry.binding = decl.binding;
    entry.visibility = WGPUShaderStage_Compute;
    switch (decl.kind) {
        case BindingKind::UniformBuffer:
            entry.buffer.type = WGPUBufferBindingType_Uniform;
            break;
        case BindingKind::StorageBufferRead:
            entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
            break;
        case BindingKind::StorageBufferReadWrite:
            entry.buffer.type = WGPUBufferBindingType_Storage;
            break;
        case BindingKind::StorageTextureWrite:
        case BindingKind::StorageTextureRead:
        case BindingKind::StorageTextureReadWrite:
            entry.storageTexture.access = decl.kind == BindingKind::StorageTextureWrite   ? WGPUStorageTextureAccess_WriteOnly
                                          : decl.kind == BindingKind::StorageTextureRead ? WGPUStorageTextureAccess_ReadOnly
                                                                                          : WGPUStorageTextureAccess_ReadWrite;
            entry.storageTexture.format = toWgpu(decl.format);
            entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
            break;
    }
    return entry;
}

}  // namespace

Result<TextureHandle> WebGpuDevice::createTexture(const TextureDesc& desc) {
    Impl& impl = *impl_;
    if (desc.extent.width == 0 || desc.extent.height == 0 || desc.extent.depth != 1) {
        return Error{ErrorCode::InvalidArgument, "Das WebGPU-Backend legt nur 2D-Texturen mit Größe > 0 an"};
    }
    impl.drainErrors();

    WGPUTextureDescriptor td{};
    td.label = view(desc.label);
    td.usage = toWgpuTexture(desc.usage);
    td.dimension = WGPUTextureDimension_2D;
    td.size = {desc.extent.width, desc.extent.height, 1};
    td.format = toWgpu(desc.format);
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    WGPUTexture texture = wgpuDeviceCreateTexture(impl.device, &td);
    WGPUTextureView textureView = texture != nullptr ? wgpuTextureCreateView(texture, nullptr) : nullptr;
    impl.pump(false);
    const std::vector<std::string> errors = impl.drainErrors();
    if (texture == nullptr || textureView == nullptr || !errors.empty()) {
        if (textureView != nullptr) wgpuTextureViewRelease(textureView);
        if (texture != nullptr) {
            wgpuTextureDestroy(texture);
            wgpuTextureRelease(texture);
        }
        return gpuError(withErrors("Die Textur ließ sich nicht anlegen", errors));
    }

    const TextureHandle handle{++impl.nextId};
    impl.textures[handle.value()] = TextureEntry{texture, textureView, desc.extent, desc.format, desc.usage};
    return handle;
}

Result<BufferHandle> WebGpuDevice::createBuffer(const BufferDesc& desc) {
    Impl& impl = *impl_;
    if (desc.sizeBytes == 0) {
        return Error{ErrorCode::InvalidArgument, "Ein Puffer braucht mindestens 1 Byte"};
    }
    if (hasFlag(desc.usage, Usage::MapRead) && hasFlag(desc.usage, Usage::Storage)) {
        return Error{ErrorCode::InvalidArgument, "MapRead darf nie zusammen mit Storage vorkommen"};
    }
    impl.drainErrors();

    const std::uint64_t size = alignUp(desc.sizeBytes, 4);
    WGPUBufferDescriptor bd{};
    bd.label = view(desc.label);
    bd.usage = toWgpuBuffer(desc.usage);
    bd.size = size;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(impl.device, &bd);
    impl.pump(false);
    const std::vector<std::string> errors = impl.drainErrors();
    if (buffer == nullptr || !errors.empty()) {
        if (buffer != nullptr) {
            wgpuBufferDestroy(buffer);
            wgpuBufferRelease(buffer);
        }
        return gpuError(withErrors("Der Puffer ließ sich nicht anlegen", errors));
    }

    const BufferHandle handle{++impl.nextId};
    impl.buffers[handle.value()] = BufferEntry{buffer, size, desc.usage, 0};
    return handle;
}

Result<PreparedPipeline> preparePipeline(WebGpuDevice::Impl& impl, const ComputePipelineDesc& desc) {
    if (desc.shader.language != ShaderLanguage::Wgsl) {
        return Error{ErrorCode::Unsupported, "Das WebGPU-Backend versteht nur WGSL"};
    }
    Result<std::vector<DeclaredBinding>> declared = readBindings(desc.shader.code);
    if (!declared.ok()) {
        return declared.error();
    }
    impl.drainErrors();

    PreparedPipeline prepared;
    prepared.bindings = std::move(declared.value());
    std::uint32_t groupCount = 0;
    for (const DeclaredBinding& d : prepared.bindings) {
        groupCount = std::max(groupCount, d.group + 1U);
    }

    // Layouts: eine Gruppe ohne Bindungen bekommt ein leeres Layout.
    prepared.layouts.assign(groupCount, nullptr);
    for (std::uint32_t group = 0; group < groupCount; ++group) {
        std::vector<WGPUBindGroupLayoutEntry> entries;
        for (const DeclaredBinding& d : prepared.bindings) {
            if (d.group == group) {
                entries.push_back(layoutEntry(d));
            }
        }
        WGPUBindGroupLayoutDescriptor ld{};
        ld.entryCount = entries.size();
        ld.entries = entries.data();
        prepared.layouts[group] = wgpuDeviceCreateBindGroupLayout(impl.device, &ld);
    }

    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = prepared.layouts.size();
    pld.bindGroupLayouts = prepared.layouts.data();
    prepared.pipelineLayout = wgpuDeviceCreatePipelineLayout(impl.device, &pld);

    // Das Modul prüft den WGSL-Text (Parser, Validierung): billig, Fehler kommen hier sofort. Teuer ist erst das
    // Übersetzen durch den Treiber beim Anlegen der Pipeline.
    WGPUShaderSourceWGSL wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code = WGPUStringView{desc.shader.code.data(), desc.shader.code.size()};
    WGPUShaderModuleDescriptor smd{};
    smd.nextInChain = &wgsl.chain;
    smd.label = view(desc.label);
    prepared.module = wgpuDeviceCreateShaderModule(impl.device, &smd);
    prepared.label = desc.label;
    prepared.entryPoint = desc.shader.entryPoint;
    prepared.codeSize = desc.shader.code.size();
    return prepared;
}

void releasePrepared(PreparedPipeline& prepared, bool keepLayouts) noexcept {
    if (prepared.module != nullptr) {
        wgpuShaderModuleRelease(prepared.module);
        prepared.module = nullptr;
    }
    if (prepared.pipelineLayout != nullptr) {
        wgpuPipelineLayoutRelease(prepared.pipelineLayout);
        prepared.pipelineLayout = nullptr;
    }
    if (!keepLayouts) {
        for (WGPUBindGroupLayout layout : prepared.layouts) {
            if (layout != nullptr) {
                wgpuBindGroupLayoutRelease(layout);
            }
        }
        prepared.layouts.clear();
    }
}

WGPUComputePipeline compilePrepared(WGPUDevice device, const PreparedPipeline& prepared) {
    WGPUComputePipelineDescriptor cpd{};
    cpd.label = view(prepared.label);
    cpd.layout = prepared.pipelineLayout;
    cpd.compute.module = prepared.module;
    cpd.compute.entryPoint = view(prepared.entryPoint);
    const auto compileStart = std::chrono::steady_clock::now();
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &cpd);
    // Fehlersuche (Issue #26): Wer im Terminal startet, sieht, wenn der Treiber lange für einen Shader braucht.
    const auto compileMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - compileStart).count();
    if (compileMs > 300) {
        std::fprintf(stderr, "Shader übersetzt in %lld ms (%zu Zeichen)\n", static_cast<long long>(compileMs), prepared.codeSize);
    }
    return pipeline;
}

ComputePipelineHandle registerPipeline(WebGpuDevice::Impl& impl, WGPUComputePipeline pipeline, PreparedPipeline& prepared) {
    const ComputePipelineHandle handle{++impl.nextId};
    impl.pipelines[handle.value()] = PipelineEntry{pipeline, std::move(prepared.layouts), std::move(prepared.bindings)};
    prepared.layouts.clear();
    return handle;
}

Result<ComputePipelineHandle> WebGpuDevice::createComputePipeline(const ComputePipelineDesc& desc) {
    Impl& impl = *impl_;
    Result<PreparedPipeline> prepared = preparePipeline(impl, desc);
    if (!prepared.ok()) {
        return prepared.error();
    }
    WGPUComputePipeline pipeline = compilePrepared(impl.device, prepared.value());
    impl.pump(true);
    const std::vector<std::string> errors = impl.drainErrors();
    releasePrepared(prepared.value(), true);
    if (pipeline == nullptr || !errors.empty()) {
        if (pipeline != nullptr) wgpuComputePipelineRelease(pipeline);
        releasePrepared(prepared.value(), false);
        return Error{ErrorCode::ShaderError, withErrors("Die Compute-Pipeline ließ sich nicht anlegen", errors)};
    }
    return registerPipeline(impl, pipeline, prepared.value());
}

void WebGpuDevice::destroy(TextureHandle handle) noexcept {
    const auto it = impl_->textures.find(handle.value());
    if (it == impl_->textures.end()) {
        return;
    }
    wgpuTextureViewRelease(it->second.view);
    wgpuTextureDestroy(it->second.texture);
    wgpuTextureRelease(it->second.texture);
    impl_->textures.erase(it);
}

void WebGpuDevice::destroy(BufferHandle handle) noexcept {
    const auto it = impl_->buffers.find(handle.value());
    if (it == impl_->buffers.end()) {
        return;
    }
    // Ein Puffer mit laufendem Readback bleibt über die Referenz des Readbacks am Leben;
    // ein ausdrückliches destroy() würde das Mapping abbrechen.
    if (it->second.pendingMaps == 0) {
        wgpuBufferDestroy(it->second.buffer);
    }
    wgpuBufferRelease(it->second.buffer);
    impl_->buffers.erase(it);
}

void WebGpuDevice::destroy(ComputePipelineHandle handle) noexcept {
    const auto it = impl_->pipelines.find(handle.value());
    if (it == impl_->pipelines.end()) {
        return;
    }
    for (WGPUBindGroupLayout layout : it->second.layouts) {
        wgpuBindGroupLayoutRelease(layout);
    }
    wgpuComputePipelineRelease(it->second.pipeline);
    impl_->pipelines.erase(it);
}

Result<void> WebGpuDevice::writeBuffer(BufferHandle buffer, std::uint64_t offsetBytes,
                                       std::span<const std::byte> data) {
    Impl& impl = *impl_;
    const auto it = impl.buffers.find(buffer.value());
    if (it == impl.buffers.end()) {
        return Error{ErrorCode::NotFound, "Unbekannter Puffer"};
    }
    if (!hasFlag(it->second.usage, Usage::CopyDestination)) {
        return Error{ErrorCode::InvalidArgument, "Der Puffer hat kein Usage::CopyDestination"};
    }
    if (offsetBytes % 4 != 0 || data.size() % 4 != 0) {
        return Error{ErrorCode::InvalidArgument, "Offset und Länge eines Schreibzugriffs müssen durch 4 teilbar sein"};
    }
    if (offsetBytes > it->second.size || data.size() > it->second.size - offsetBytes) {
        return Error{ErrorCode::InvalidArgument, "Der Schreibzugriff liegt außerhalb des Puffers"};
    }
    if (data.empty()) {
        return {};
    }
    wgpuQueueWriteBuffer(impl.queue, it->second.buffer, offsetBytes, data.data(), data.size());
    return {};
}

}  // namespace whitehole::gpu
