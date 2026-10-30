// gpu_types.hpp: Handles und Beschreibungen für GPU-Ressourcen.
// Bewusst frei von Backend-Typen (Dawn, wgpu): Der Rest der Engine kennt nur
// diese opaken Handles.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/types.hpp"

namespace whitehole::gpu {

struct TextureTag {};
struct BufferTag {};
struct ComputePipelineTag {};

using TextureHandle = StrongId<TextureTag>;
using BufferHandle = StrongId<BufferTag>;
using ComputePipelineHandle = StrongId<ComputePipelineTag>;

enum class TextureFormat : std::uint8_t { Rgba8Unorm, Rgba16Float, Rgba32Float, R32Float };

/// Verwendungszweck einer Ressource (Bit-Flags, kombinierbar mit |).
enum class Usage : std::uint32_t {
    None = 0,
    Sampled = 1U << 0,
    Storage = 1U << 1,
    Uniform = 1U << 2,
    CopySource = 1U << 3,
    CopyDestination = 1U << 4,
    MapRead = 1U << 5,  // CPU-lesbar; darf nur zusammen mit CopyDestination vorkommen, nie als Storage
};

[[nodiscard]] constexpr Usage operator|(Usage a, Usage b) noexcept {
    return static_cast<Usage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr bool hasFlag(Usage set, Usage flag) noexcept {
    return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(flag)) != 0U;
}

struct TextureDesc {
    Extent3 extent;
    TextureFormat format{TextureFormat::Rgba16Float};
    Usage usage{Usage::Sampled | Usage::Storage};
    std::string label;
};

struct BufferDesc {
    std::uint64_t sizeBytes{0};
    Usage usage{Usage::Storage};
    std::string label;
};

enum class ShaderLanguage : std::uint8_t { Wgsl, Glsl };

struct ShaderSource {
    ShaderLanguage language{ShaderLanguage::Wgsl};
    std::string code;
    std::string entryPoint{"main"};
};

struct ComputePipelineDesc {
    ShaderSource shader;
    std::string label;
};

/// Bindet eine Ressource an `@group(group) @binding(slot)` der Compute-Pipeline.
/// Konvention des Feldcompilers: Gruppe 0 gehört der Feldbibliothek (Frame-Daten,
/// Parameter), Konsumenten wie der Canvas-Pass nehmen Gruppe 1 und höher. Ob eine
/// Ressource als Uniform, Storage oder Textur gebunden wird, ergibt sich aus ihrem
/// Usage (Buffer) beziehungsweise ihrem Handle-Typ.
struct Binding {
    std::uint32_t group{0};
    std::uint32_t slot{0};
    std::variant<TextureHandle, BufferHandle> resource;
};

struct DispatchDesc {
    ComputePipelineHandle pipeline;
    std::span<const Binding> bindings;
    std::uint32_t groupsX{1};
    std::uint32_t groupsY{1};
    std::uint32_t groupsZ{1};
};

}  // namespace whitehole::gpu
