// Wellenform des Audio-Eingangs als Node: die Grundlage des Oszilloskops.
//
// Warum es diese Datei gibt: Pegel, Bänder und Spektrum (builtin_audio.cpp) sind Kennzahlen; ein
// Oszilloskop zeigt dagegen die Kurve selbst. Der Kernel holt aus dem AudioStore die letzten Samples
// (AudioFrame::recent) und schneidet daraus ein Fenster von `span` Millisekunden. Das Fenster wird
// in 32 Stücke geteilt; von jedem bleibt der Sample mit dem größten Betrag (mit Vorzeichen), damit
// auch schnelle Töne ihre Höhe behalten (ein Mittelwert würde sie wegglätten). Die 32 Punkte gehen
// als Porttyp AudioWave auf demselben Weg zur GPU wie das Spektrum; das Feld `wh.generator.scope`
// zeichnet sie.
//
// Trigger: Wie bei einem echten Oszilloskop beginnt das Fenster an einem steigenden Nulldurchgang
// (genauer: dem Durchgang durch `level`), sonst zittert das Bild von Frame zu Frame. Findet sich
// keiner, läuft das Fenster frei. Ohne Signal ist die Kurve flach (0).
#include <algorithm>
#include <cmath>
#include <memory>

#include "builtin_internal.hpp"
#include "whitehole/audio/audio_store.hpp"

namespace whitehole::nodes::detail {

namespace {

using audio::AudioStore;
using values::KernelContext;
using values::KernelOutputs;

class AudioWaveKernel final : public values::ICpuKernel {
public:
    explicit AudioWaveKernel(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const auto frame = freshAudioFrame(*store_);

        Spectrum target;  // alles 0: flache Linie
        if (frame && !frame->recent.empty()) {
            const std::vector<float>& samples = frame->recent;
            const double spanMs = std::clamp(in.number("span", 30.0), 2.0, 100.0);
            // Das Fenster darf höchstens die Hälfte der Samples füllen: davor liegt der Platz für die Triggersuche.
            const std::size_t window = std::clamp<std::size_t>(
                static_cast<std::size_t>(spanMs / 1000.0 * frame->sampleRate), kSpectrumBands, samples.size() / 2);
            const std::size_t end = samples.size();
            std::size_t start = end - window;
            if (in.boolean("trigger", true)) {
                const auto level = static_cast<float>(std::clamp(in.number("level", 0.0), -1.0, 1.0));
                // Letzter steigender Durchgang im Bereich [end - 2*window, end - window): dann liegt das
                // ganze Fenster dahinter in den vorhandenen Samples.
                for (std::size_t k = end - window; k > end - 2 * window && k > 0; --k) {
                    if (samples[k - 1] < level && samples[k] >= level) {
                        start = k;
                        break;
                    }
                }
            }
            const double gain = in.number("gain", 1.0);
            for (std::size_t i = 0; i < kSpectrumBands; ++i) {
                const std::size_t from = start + i * window / kSpectrumBands;
                const std::size_t to = std::max(start + (i + 1) * window / kSpectrumBands, from + 1);
                float strongest = 0.0F;
                for (std::size_t k = from; k < to && k < end; ++k) {
                    if (std::fabs(samples[k]) > std::fabs(strongest)) {
                        strongest = samples[k];
                    }
                }
                target.bands[i] = static_cast<float>(std::clamp(static_cast<double>(strongest) * gain, -1.0, 1.0));
            }
        }

        // Zeitliche Glättung: 0 = jedes Bild neu, nahe 1 = träge. Ohne Signal läuft die Kurve so sanft auf 0.
        const auto smooth = static_cast<float>(std::clamp(in.number("smooth", 0.0), 0.0, 0.95));
        for (std::size_t i = 0; i < kSpectrumBands; ++i) {
            shown_.bands[i] = shown_.bands[i] * smooth + target.bands[i] * (1.0F - smooth);
        }
        outputs.setSpectrum("wave", shown_);
        outputs.set("present", frame.has_value());
    }

private:
    std::shared_ptr<AudioStore> store_;
    Spectrum shown_;
};

}  // namespace

std::vector<values::CpuNodeDefinition> audioWaveNodes(std::shared_ptr<audio::AudioStore> store) {
    using graph::PortType;
    if (!store) {
        store = std::make_shared<AudioStore>();  // ohne Quelle: dauerhaft "kein Signal"
    }
    std::vector<values::CpuNodeDefinition> nodes;
    nodes.push_back(cpuNode("wh.value.audiowave", "Audio Waveform", "Audio",
                            {valueOutput("wave", "Wave", PortType::AudioWave), valueOutput("present", "Signal", PortType::Boolean)},
                            {number("gain", "Gain", 1.0, 0.0, 50.0), number("span", "Window (ms)", 30.0, 2.0, 100.0),
                             toggle("trigger", "Trigger on rising edge", true), number("level", "Trigger level", 0.0, -1.0, 1.0),
                             number("smooth", "Smoothing", 0.0, 0.0, 0.95)},
                            [store] { return std::make_unique<AudioWaveKernel>(store); }));
    return nodes;
}

}  // namespace whitehole::nodes::detail
