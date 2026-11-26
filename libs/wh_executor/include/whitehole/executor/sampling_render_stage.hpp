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
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/mapping/led_map.hpp"

namespace whitehole::executor {

inline constexpr std::uint64_t kBytesPerLed = 16;  // vec4<f32>

struct SamplingStageConfig {
    graph::PortRef target;  // Field-Ausgang, der abgetastet wird
    std::uint32_t ledCapacity{0};
    std::string label{"wh.leds"};
    /// Platz für einen Zusatzausgang (geteilte Vorschau): der Farbpuffer wird um einen Kopf und `ledCapacity`
    /// Farben länger (siehe sampling_shader.hpp). Ohne ihn werden Zusatzausgänge von setGraph abgelehnt.
    bool secondary{false};
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

    /// Siehe FieldPass::setGraph. `extras` nur mit SamplingStageConfig::secondary (sonst InvalidArgument).
    Result<void> setGraph(const graph::Graph& graph, std::span<const graph::PortRef> extras = {});
    /// Wählt, welcher Zusatzausgang (Stelle in `extras` des letzten setGraph) ab dem nächsten Frame mit abgetastet wird;
    /// kNoSamplingExtra = keiner. `tag` steht im Kopf des Zusatzbereichs (siehe sampling_shader.hpp). Fehler: GpuError.
    Result<void> selectExtra(std::uint32_t extra, std::uint32_t tag);
    /// Siehe FieldPass::setParameters (nur Werte geändert, kein Neuübersetzen).
    Result<void> setParameters(const graph::Graph& graph) { return pass_->setParameters(graph); }
    /// Neue Strukturen im Hintergrund übersetzen (FieldPass::setBackgroundCompile).
    void setBackgroundCompile(bool on) noexcept { pass_->setBackgroundCompile(on); }
    /// Wartet eine neue Struktur noch auf den Treiber (bis dahin läuft der bisherige Graph)?
    [[nodiscard]] bool graphPending() const noexcept { return pass_->pending(); }
    /// Siehe FieldPass::prefetch. `extras` wie bei setGraph.
    Result<void> prefetch(const graph::Graph& graph, std::span<const graph::PortRef> extras = {});
    /// Siehe FieldPass::cancelPrefetches.
    void cancelPrefetches() noexcept { pass_->cancelPrefetches(); }
    [[nodiscard]] std::size_t prefetchesOpen() const noexcept { return pass_->prefetchesOpen(); }

    void record(const FrameTime& time, gpu::ICommandRecorder& recorder) override;

    /// Farben aller LEDs (Storage, CopySource). Das Handle ändert sich nie.
    [[nodiscard]] gpu::BufferHandle output() const noexcept { return colors_; }
    /// Größe des Ausgabepuffers: ledCapacity * 16 Byte, mit Zusatzbereich (2 * ledCapacity + 1) * 16 Byte.
    [[nodiscard]] std::uint64_t outputSizeBytes() const noexcept {
        return (secondary_ ? 2 * capacity_ + 1 : capacity_) * kBytesPerLed;
    }
    /// Nur die Farben des Hauptausgangs: ledCapacity * 16 Byte (der Anfang des Ausgabepuffers).
    [[nodiscard]] std::uint64_t mainSizeBytes() const noexcept { return capacity_ * kBytesPerLed; }
    /// Ob es den Zusatzbereich gibt (SamplingStageConfig::secondary).
    [[nodiscard]] bool hasSecondary() const noexcept { return secondary_; }
    /// Anzahl der gültigen LEDs (0 bis zum ersten setMap).
    [[nodiscard]] std::uint32_t ledCount() const noexcept { return ledCount_; }

    [[nodiscard]] const std::optional<Error>& graphError() const noexcept { return pass_->graphError(); }
    [[nodiscard]] const std::optional<Error>& frameError() const noexcept { return pass_->frameError(); }
    [[nodiscard]] StageStats stats() const noexcept;
    /// Die CPU-Werte des letzten Frames (für die Live-Anzeige). Nur vom Frame-Thread aus lesen.
    [[nodiscard]] const values::ValueStore& values() const noexcept { return pass_->values(); }

private:
    SamplingRenderStage(gpu::IGpuDevice& device, std::uint64_t capacity, bool secondary, gpu::BufferHandle positions,
                        gpu::BufferHandle colors, gpu::BufferHandle info,
                        std::unique_ptr<FieldPass> pass);

    Result<void> writeInfo(std::uint32_t count);

    gpu::IGpuDevice& device_;
    std::uint64_t capacity_;
    bool secondary_;
    std::uint32_t extra_{0xFFFFFFFFU};
    std::uint32_t tag_{0};
    gpu::BufferHandle positions_;
    gpu::BufferHandle colors_;
    gpu::BufferHandle info_;
    std::unique_ptr<FieldPass> pass_;
    std::uint32_t ledCount_{0};
    std::uint64_t framesWithoutMap_{0};
};

}  // namespace whitehole::executor
