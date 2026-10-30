// sampling_render_stage.hpp: tastet eine Field-Kette an den Positionen der LEDs ab.
//
// Warum es diese Datei gibt: Das ist die eigentliche Brücke vom Effekt zur Hardware.
// Die LedMap (wh_mapping) sagt, wo jede LED im Raum sitzt; diese Stage fragt den Effekt
// an genau diesen Punkten ab und liefert eine Farbe je LED. Sie funktioniert für 2D
// und 3D gleichermaßen, weil ein Effekt nur eine Funktion der Position ist.
//
// Aufgabe: besitzt Positions-, Farb- und Info-Puffer; setMap lädt die Positionen hoch;
// record tastet ab. Der gemeinsame Ablauf (CPU-Nodes, Parameter, Pipeline, Fehler)
// steckt im FieldPass.
//
// Ausgabe: `output()` ist ein Puffer mit je 16 Byte (linear RGBA, f32) pro LED. Das ist
// genau die Form, die RenderEngine in den Readback-Ring kopiert
// (RenderEngineConfig::outputBuffer / outputSizeBytes). Gamma, Farbreihenfolge und das
// Packen in Universen erledigt der Ausgabe-Pass in Schritt 6.
//
// Kapazität: Die Puffer werden für `ledCapacity` LEDs angelegt, damit das Handle der
// Ausgabe stabil bleibt (die Engine bekommt es einmal). Ein Layout mit weniger LEDs ist
// jederzeit erlaubt; ein größeres braucht eine neue Stage und einen neuen Engine-Aufbau.
// Hinter den letzten gültigen LEDs steht im Ausgabepuffer alter Inhalt; der Ausgabe-Pass
// liest nur `ledCount()` Einträge.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/engine/render_stage.hpp"
#include "whitehole/executor/field_pass.hpp"
#include "whitehole/mapping/led_map.hpp"

namespace whitehole::executor {

inline constexpr std::uint64_t kBytesPerLed = 16;  // vec4<f32>

struct SamplingStageConfig {
    graph::PortRef target;  // Field-Ausgang, der abgetastet wird
    std::uint32_t ledCapacity{0};
    std::string label{"wh.leds"};
};

/// Packt Positionen als vec4<f32> (x, y, z, 0), wie der Shader sie liest.
[[nodiscard]] std::vector<std::byte> packPositions(std::span<const Vec3> positions);

class SamplingRenderStage final : public engine::IRenderStage {
public:
    /// Fehler: InvalidArgument (Kapazität 0 oder über kMaxLedsPerLayout), GpuError.
    /// Bei einem Fehler bleibt nichts auf dem Gerät liegen.
    [[nodiscard]] static Result<std::unique_ptr<SamplingRenderStage>> create(
        gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
        const values::ICpuKernelLibrary& kernels, SamplingStageConfig config);

    ~SamplingRenderStage() override;
    SamplingRenderStage(const SamplingRenderStage&) = delete;
    SamplingRenderStage& operator=(const SamplingRenderStage&) = delete;

    /// Lädt die Einheitskoordinaten (`map.unit`) hoch.
    /// Fehler: InvalidArgument (leer oder mehr LEDs als die Kapazität), GpuError.
    /// Bei einem Fehler bleibt die bisherige Karte aktiv.
    Result<void> setMap(const mapping::LedMap& map);

    /// Siehe FieldPass::setGraph.
    Result<void> setGraph(const graph::Graph& graph) { return pass_->setGraph(graph); }

    void record(const FrameTime& time, gpu::ICommandRecorder& recorder) override;

    /// Farben aller LEDs (Storage, CopySource). Das Handle ändert sich nie.
    [[nodiscard]] gpu::BufferHandle output() const noexcept { return colors_; }
    /// Größe des Ausgabepuffers: ledCapacity * 16 Byte.
    [[nodiscard]] std::uint64_t outputSizeBytes() const noexcept { return capacity_ * kBytesPerLed; }
    /// Anzahl der gültigen LEDs (0 bis zum ersten setMap).
    [[nodiscard]] std::uint32_t ledCount() const noexcept { return ledCount_; }

    [[nodiscard]] const std::optional<Error>& graphError() const noexcept { return pass_->graphError(); }
    [[nodiscard]] const std::optional<Error>& frameError() const noexcept { return pass_->frameError(); }
    [[nodiscard]] StageStats stats() const noexcept;

private:
    SamplingRenderStage(gpu::IGpuDevice& device, std::uint64_t capacity, gpu::BufferHandle positions,
                        gpu::BufferHandle colors, gpu::BufferHandle info,
                        std::unique_ptr<FieldPass> pass);

    gpu::IGpuDevice& device_;
    std::uint64_t capacity_;
    gpu::BufferHandle positions_;
    gpu::BufferHandle colors_;
    gpu::BufferHandle info_;
    std::unique_ptr<FieldPass> pass_;
    std::uint32_t ledCount_{0};
    std::uint64_t framesWithoutMap_{0};
};

}  // namespace whitehole::executor
