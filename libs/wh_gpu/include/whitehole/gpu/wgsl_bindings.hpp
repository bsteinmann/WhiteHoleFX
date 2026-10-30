// wgsl_bindings.hpp: liest aus WGSL-Text, welche Ressourcen ein Shader deklariert.
//
// Warum es diese Datei gibt: Ein WebGPU-Backend braucht für jede Compute-Pipeline ein
// Layout, das sagt, was an welchem @group/@binding hängt (Uniform, Storage-Puffer,
// Storage-Textur). WebGPU kann das Layout selbst aus dem Shader ableiten ("auto"), aber
// nur aus den Bindungen, die der Shader tatsächlich benutzt. Deklariert die Feldbibliothek
// einen Parameterpuffer, den ein konstanter Graph nie liest, fehlt er im automatischen
// Layout, und der Bind-Group-Aufbau der Engine, der immer alle Bindungen setzt, schlägt
// fehl. Mit den hier gelesenen Deklarationen baut das Backend das Layout selbst,
// vollständig und unabhängig davon, was der Shader gerade benutzt.
//
// Aufgabe: Alle `@group(g) @binding(b) var ...`-Deklarationen finden und je Bindung die
// Art bestimmen. Unterstützt ist genau, was der Feldcompiler und die Konsumenten
// erzeugen: Uniform, Storage (read / read_write) und texture_storage_2d. Anderes (Sampler,
// Texturen zum Abtasten, Bindungs-Arrays) wird mit einem Fehler abgelehnt, statt
// stillschweigend falsch gelesen zu werden. Die Attribute müssen in der Reihenfolge
// @group, @binding stehen, wie sie der Generator schreibt. Kommentare werden ignoriert.
//
// Abhängigkeiten: nur wh_gpu-Typen. Keine GPU, kein Backend: vollständig testbar.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/gpu/gpu_types.hpp"

namespace whitehole::gpu {

enum class BindingKind : std::uint8_t {
    UniformBuffer,
    StorageBufferRead,       // var<storage, read> (auch var<storage>)
    StorageBufferReadWrite,  // var<storage, read_write>
    StorageTextureWrite,     // texture_storage_2d<fmt, write>
    StorageTextureRead,
    StorageTextureReadWrite,
};

struct DeclaredBinding {
    std::uint32_t group{0};
    std::uint32_t binding{0};
    BindingKind kind{BindingKind::UniformBuffer};
    TextureFormat format{TextureFormat::Rgba8Unorm};  // nur bei Storage-Texturen
    std::string name;

    bool operator==(const DeclaredBinding&) const = default;
};

/// Liefert alle Bindungen, sortiert nach (group, binding).
/// Fehler: ShaderError bei einer nicht unterstützten Deklaration, doppelter Bindung
/// oder Text, den der Leser nicht versteht.
[[nodiscard]] Result<std::vector<DeclaredBinding>> readBindings(std::string_view wgsl);

}  // namespace whitehole::gpu
