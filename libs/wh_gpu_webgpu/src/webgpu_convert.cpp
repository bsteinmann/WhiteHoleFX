#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "webgpu_impl.hpp"

namespace whitehole::gpu {

std::uint32_t bytesPerTexel(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Rgba8Unorm: return 4;
        case TextureFormat::Rgba16Float: return 8;
        case TextureFormat::Rgba32Float: return 16;
        case TextureFormat::R32Float: return 4;
    }
    return 4;
}

std::uint32_t paddedBytesPerRow(std::uint32_t width, TextureFormat format) noexcept {
    return static_cast<std::uint32_t>(alignUp(std::uint64_t{width} * bytesPerTexel(format), 256));
}

std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) noexcept {
    return (value + alignment - 1U) / alignment * alignment;
}

WGPUStringView view(const std::string& s) noexcept {
    return WGPUStringView{s.data(), s.size()};
}

std::string toString(WGPUStringView s) {
    if (s.data == nullptr) {
        return {};
    }
    return s.length == WGPU_STRLEN ? std::string{s.data} : std::string{s.data, s.length};
}

WGPUTextureFormat toWgpu(TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Rgba8Unorm: return WGPUTextureFormat_RGBA8Unorm;
        case TextureFormat::Rgba16Float: return WGPUTextureFormat_RGBA16Float;
        case TextureFormat::Rgba32Float: return WGPUTextureFormat_RGBA32Float;
        case TextureFormat::R32Float: return WGPUTextureFormat_R32Float;
    }
    return WGPUTextureFormat_Undefined;
}

WGPUTextureUsage toWgpuTexture(Usage usage) noexcept {
    WGPUTextureUsage out = WGPUTextureUsage_None;
    if (hasFlag(usage, Usage::Sampled)) out |= WGPUTextureUsage_TextureBinding;
    if (hasFlag(usage, Usage::Storage)) out |= WGPUTextureUsage_StorageBinding;
    if (hasFlag(usage, Usage::CopySource)) out |= WGPUTextureUsage_CopySrc;
    if (hasFlag(usage, Usage::CopyDestination)) out |= WGPUTextureUsage_CopyDst;
    return out;
}

WGPUBufferUsage toWgpuBuffer(Usage usage) noexcept {
    WGPUBufferUsage out = WGPUBufferUsage_None;
    if (hasFlag(usage, Usage::Uniform)) out |= WGPUBufferUsage_Uniform;
    if (hasFlag(usage, Usage::Storage)) out |= WGPUBufferUsage_Storage;
    if (hasFlag(usage, Usage::CopySource)) out |= WGPUBufferUsage_CopySrc;
    if (hasFlag(usage, Usage::CopyDestination)) out |= WGPUBufferUsage_CopyDst;
    if (hasFlag(usage, Usage::MapRead)) out |= WGPUBufferUsage_MapRead;
    return out;
}

std::string checkResource(const DeclaredBinding& decl, const Binding& binding, const BufferEntry* buffer,
                          const TextureEntry* texture) {
    const std::string where = "@group(" + std::to_string(decl.group) + ") @binding(" +
                              std::to_string(decl.binding) + ") " + decl.name + ": ";
    const bool wantsTexture = decl.kind == BindingKind::StorageTextureWrite ||
                              decl.kind == BindingKind::StorageTextureRead ||
                              decl.kind == BindingKind::StorageTextureReadWrite;
    if (wantsTexture) {
        if (texture == nullptr) {
            return where + "erwartet eine Textur";
        }
        if (!hasFlag(texture->usage, Usage::Storage)) {
            return where + "Textur ohne Usage::Storage";
        }
        if (texture->format != decl.format) {
            return where + "Textur-Format passt nicht zur Deklaration";
        }
        return {};
    }
    if (buffer == nullptr) {
        return where + "erwartet einen Puffer";
    }
    const Usage need = decl.kind == BindingKind::UniformBuffer ? Usage::Uniform : Usage::Storage;
    if (!hasFlag(buffer->usage, need)) {
        return where + (need == Usage::Uniform ? "Puffer ohne Usage::Uniform" : "Puffer ohne Usage::Storage");
    }
    (void)binding;
    return {};
}

}  // namespace whitehole::gpu
