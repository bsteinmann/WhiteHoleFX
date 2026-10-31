// parameter_layout.hpp: Lage der Node-Parameter im Uniform-Puffer des Feldshaders.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/graph/value_lookup.hpp"

namespace whitehole::compiler {

/// GPU-Typen, die ein Parameter annehmen kann. Bool, Ganz- und Gleitkommazahlen
/// gehen als f32 auf die GPU, Farben als vec4<f32>. Texte haben keine
/// GPU-Entsprechung und kommen nicht in den Puffer.
enum class GpuType : std::uint8_t {
    Float,
    Vec2,
    Vec3,
    Vec4,
    /// 32 Spektrumbänder als array<vec4<f32>, 8> (128 Bytes). Kommt nie aus einem ParameterValue,
    /// sondern über IValueLookup::spectrum (SlotOrigin::SpectrumInput).
    Spectrum,
};

[[nodiscard]] std::optional<GpuType> gpuTypeOf(const graph::ParameterValue& value) noexcept;
[[nodiscard]] std::string_view wgslTypeName(GpuType type) noexcept;

/// Woher der Wert eines Slots kommt.
enum class SlotOrigin : std::uint8_t {
    /// Parameter des Nodes: gespeicherter Wert, oder der Wert der Quelle, wenn der
    /// gleichnamige Eingang verbunden ist (Modulation, siehe parameter_resolve.hpp).
    Parameter,
    /// Ein Scalar oder eine Farbe, die an einem Field-Eingang hängt und dort als
    /// konstantes Feld wirkt. `key` ist der Schlüssel des Eingangs. Ohne Verbindung
    /// oder Wert bleibt der Slot null.
    FieldConstant,
    /// Ein Spektrum, das an einem Eingang vom Typ AudioSpectrum hängt. `key` ist der Schlüssel
    /// des Eingangs. Ohne Verbindung oder Wert bleibt der Slot null (kein Ton).
    SpectrumInput,
};

struct ParameterSlot {
    NodeId node;
    std::string key;
    GpuType type{GpuType::Float};
    std::uint32_t offsetBytes{0};
    std::string member;  // Name des Struct-Members im Shader, z. B. "n7_gain"
    SlotOrigin origin{SlotOrigin::Parameter};
};

struct ParameterLayout {
    std::vector<ParameterSlot> slots;
    /// Größe des Puffers in Bytes (Vielfaches von 16, inklusive des Abschlusselements).
    std::uint32_t sizeBytes{0};
};

/// Frame-Daten am Binding 0 (WGSL: struct WhFrame { time, delta, frame, _pad }).
struct FrameUniforms {
    float time{0.0F};
    float delta{0.0F};
    std::uint32_t frame{0};
    std::uint32_t padding{0};
};
static_assert(sizeof(FrameUniforms) == 16, "FrameUniforms muss dem WGSL-Struct WhFrame entsprechen");

/// Vergibt die Offsets nach den Layout-Regeln von WGSL (f32: 4/4, vec2: 8/8,
/// vec3: 12 Bytes mit Ausrichtung 16, vec4: 16/16; Größe/Ausrichtung).
class LayoutBuilder {
public:
    void add(NodeId node, std::string key, GpuType type,
             SlotOrigin origin = SlotOrigin::Parameter);

    /// Hängt ein vec4-Abschlusselement an. Es sorgt dafür, dass das WGSL-Struct
    /// nie leer ist und die Größe ein Vielfaches von 16 bleibt.
    [[nodiscard]] ParameterLayout build() const;

private:
    std::vector<ParameterSlot> slots_;
    std::uint32_t end_{0};
};

/// Struct-Name, unter dem das Layout im Shader auftaucht.
[[nodiscard]] std::string memberName(NodeId node, std::string_view key);

/// WGSL-Deklaration `struct WhParams { ... }` passend zum Layout.
[[nodiscard]] std::string wgslParameterStruct(const ParameterLayout& layout);

/// Füllt den Uniform-Puffer mit den aktuellen Werten des Graphen.
///
/// Parameter-Slots erhalten den gespeicherten Wert oder, bei verbundenem
/// gleichnamigem Eingang, den Wert der Quelle (`values`; nullptr heißt: immer der
/// gespeicherte Wert). Konstanten-Slots erhalten den Wert ihrer Quelle.
/// Fehlt ein Node, ein Wert oder passt der Typ nicht mehr, bleiben die Bytes des
/// Slots null. Ändert nur Werte, nie die Struktur: Dafür muss nichts neu
/// kompiliert werden, auch nicht beim Verbinden oder Trennen von Modulationen.
[[nodiscard]] std::vector<std::byte> packParameters(const graph::Graph& graph,
                                                    const ParameterLayout& layout,
                                                    const graph::IValueLookup* values = nullptr);

}  // namespace whitehole::compiler
