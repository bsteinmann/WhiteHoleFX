// Audio-Eingang als Nodes: Pegel, Frequenzband, Beat, Audio Trigger (Kick/Snare/Hat/eigener Bereich), Tap-Tempo und Spektrum.
//
// Warum es diese Datei gibt: Die Analyse (wh_audio) kennt keine Nodes. Hier hängen die Kernel am
// gemeinsamen AudioStore, holen sich pro Frame den aktuellen Zustand und machen daraus Werte, die
// sich zum Modulieren eignen: geglättet (Attack/Release), mit Verstärkung und Schwelle.
//
// Wie bei DMX sind diese Nodes nicht rein: Der Wert hängt vom Eingangssignal ab, nicht von der Zeit
// allein; Offline-Rendern mit Audio-Nodes ist daher nicht reproduzierbar. Ohne Signal (nichts
// geschrieben oder älter als eine halbe Sekunde) laufen alle Werte über ihr Release gegen 0.
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include "builtin_internal.hpp"
#include "whitehole/audio/audio_store.hpp"

namespace whitehole::nodes::detail {

namespace {

using audio::AudioStore;
using values::KernelContext;
using values::KernelInputs;
using values::KernelOutputs;

/// Der aktuelle Frame, wenn Signal da ist (älter als eine halbe Sekunde zählt als "kein Signal").
std::optional<audio::AudioFrame> freshFrame(const AudioStore& store) { return freshAudioFrame(store); }

/// Tiefpass mit getrennter Zeit für Anstieg (attack) und Abfall (release), in Sekunden.
/// Zeit 0 heißt: sofort.
struct Follower {
    double value{0.0};
    double step(double target, double attack, double release, double dt) {
        const double tau = target > value ? attack : release;
        if (tau <= 1e-6 || dt <= 0.0) {
            value = target;
        } else {
            value += (target - value) * (1.0 - std::exp(-dt / tau));
        }
        return value;
    }
};

/// Bringt einen Wert nach Verstärkung und Schwelle ("floor") in 0..1.
double shape(double value, double gain, double floor) {
    const double gated = (value - floor) / std::max(1.0 - floor, 0.05);
    return std::clamp(gated * gain, 0.0, 1.0);
}

/// dBFS-Skala: -60 dB = 0, 0 dB = 1 (wie das Spektrum).
double linearToDbScale(double linear) {
    if (linear <= 1e-6) {
        return 0.0;
    }
    return std::clamp((20.0 * std::log10(linear) - audio::kFloorDb) / -audio::kFloorDb, 0.0, 1.0);
}

// --- Pegel ----------------------------------------------------------------------------------

class AudioLevelKernel final : public values::ICpuKernel {
public:
    explicit AudioLevelKernel(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double dt = context.time.delta.count();
        const auto frame = freshFrame(*store_);

        double level = 0.0;
        double peak = 0.0;
        if (frame) {
            const bool db = in.integer("scale", 1) == 1;
            level = db ? linearToDbScale(frame->rms) : frame->rms;
            peak = db ? linearToDbScale(frame->peak) : frame->peak;
        }
        if (in.boolean("auto", false)) {
            // Automatische Anpassung: der Referenzwert folgt dem Maximum und sinkt langsam (10 s).
            reference_ = std::max({level, reference_ * std::exp(-dt / 10.0), 0.05});
            level /= reference_;
            peak /= reference_;
        }
        const double gain = in.number("gain", 1.0);
        const double attack = in.number("attack", 0.01);
        const double release = in.number("release", 0.25);
        outputs.set("level", levelFollower_.step(std::clamp(level * gain, 0.0, 1.0), attack, release, dt));
        outputs.set("peak", peakFollower_.step(std::clamp(peak * gain, 0.0, 1.0), attack, release, dt));
        outputs.set("present", frame.has_value());
    }

private:
    std::shared_ptr<AudioStore> store_;
    Follower levelFollower_;
    Follower peakFollower_;
    double reference_{0.05};
};

// --- Frequenzband -----------------------------------------------------------------------------

/// Die Bänder [first, last], die den Bereich [from, to] Hz berühren; liegt er zwischen zwei Grenzen oder außerhalb, das
/// nächste Band. Der Editor hebt genau diese Bänder hervor.
std::pair<std::size_t, std::size_t> bandRange(double from, double to) {
    const auto& edges = audio::bandEdgesHz();
    std::size_t first = kSpectrumBands;
    std::size_t last = 0;
    for (std::size_t i = 0; i < kSpectrumBands; ++i) {
        if (edges[i + 1] > from && edges[i] < to) {
            first = std::min(first, i);
            last = i;
        }
    }
    if (first == kSpectrumBands) {
        const double center = std::sqrt(std::max(from, 1.0) * std::max(to, 1.0));
        std::size_t nearest = center < edges.front() ? 0 : kSpectrumBands - 1;
        for (std::size_t i = 0; i < kSpectrumBands; ++i) {
            if (center >= edges[i] && center < edges[i + 1]) {
                nearest = i;
            }
        }
        return {nearest, nearest};
    }
    return {first, last};
}

/// Größter Wert unter allen Bändern, die den Bereich [from, to] Hz berühren.
double bandValue(const Spectrum& spectrum, double from, double to) {
    const auto [first, last] = bandRange(from, to);
    double best = 0.0;
    for (std::size_t i = first; i <= last; ++i) {
        best = std::max(best, static_cast<double>(spectrum.bands[i]));
    }
    return best;
}

class AudioBandKernel final : public values::ICpuKernel {
public:
    explicit AudioBandKernel(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double dt = context.time.delta.count();
        const auto frame = freshFrame(*store_);

        const double from = std::min(in.number("from", 60.0), in.number("to", 250.0));
        const double to = std::max(in.number("from", 60.0), in.number("to", 250.0));
        double raw = 0.0;
        if (frame) {
            raw = bandValue(frame->spectrum, from, to);
        }
        const double gain = in.number("gain", 1.0);
        const double floor = in.number("floor", 0.0);
        const double shaped = shape(raw, gain, floor);

        // Anzeigestreifen: das ganze Spektrum nach Verstärkung und Schwelle, der ausgewertete Bereich markiert.
        MonitorView view;
        const auto [first, last] = bandRange(from, to);
        view.rangeFirst = static_cast<int>(first);
        view.rangeLast = static_cast<int>(last);
        if (frame) {
            for (std::size_t i = 0; i < kSpectrumBands; ++i) {
                view.bands.bands[i] = static_cast<float>(shape(static_cast<double>(frame->spectrum.bands[i]), gain, floor));
            }
        }
        outputs.setMonitor(view);
        outputs.set("value", follower_.step(shaped, in.number("attack", 0.01), in.number("release", 0.2), dt));
        outputs.set("present", frame.has_value());
    }

private:
    std::shared_ptr<AudioStore> store_;
    Follower follower_;
};

// --- Spektrum ---------------------------------------------------------------------------------

class AudioSpectrumKernel final : public values::ICpuKernel {
public:
    explicit AudioSpectrumKernel(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double dt = context.time.delta.count();
        const auto frame = freshFrame(*store_);
        const double gain = in.number("gain", 1.0);
        const double floor = in.number("floor", 0.0);
        const double attack = in.number("attack", 0.01);
        const double release = in.number("release", 0.2);

        Spectrum out;
        for (std::size_t i = 0; i < kSpectrumBands; ++i) {
            const double raw = frame ? static_cast<double>(frame->spectrum.bands[i]) : 0.0;
            out.bands[i] = static_cast<float>(followers_[i].step(shape(raw, gain, floor), attack, release, dt));
        }
        outputs.setSpectrum("spectrum", out);
        MonitorView view;
        view.bands = out;
        outputs.setMonitor(view);
        outputs.set("present", frame.has_value());
    }

private:
    std::shared_ptr<AudioStore> store_;
    std::array<Follower, kSpectrumBands> followers_{};
};

// --- Beat -------------------------------------------------------------------------------------

/// Gemeinsame Ausgabe für Beat und Tap-Tempo: Puls, Phase und Trigger aus Schlagzeit und Tempo.
struct BeatClock {
    double lastBeat{-1e9};  // Engine-Zeit des letzten Schlags

    /// `beatNow`: in diesem Bild fällt ein Schlag, zur Zeit `beatTime`.
    void emit(const KernelContext& context, bool beatNow, double beatTime, double bpm, double decay,
              KernelOutputs& outputs) {
        const double now = context.time.time.count();
        if (beatNow) {
            lastBeat = beatTime;
        }
        const double elapsed = std::max(0.0, now - lastBeat);
        const double pulse = decay > 1e-6 ? std::exp(-elapsed / decay) : (beatNow ? 1.0 : 0.0);
        const double phase = bpm > 0.0 ? std::fmod(elapsed * bpm / 60.0, 1.0) : 0.0;
        outputs.set("beat", beatNow);
        outputs.set("pulse", pulse);
        outputs.set("phase", phase);
        outputs.set("bpm", bpm);
    }
};

class AudioBeatKernel final : public values::ICpuKernel {
public:
    explicit AudioBeatKernel(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const auto frame = freshFrame(*store_);

        bool beatNow = false;
        double bpm = 0.0;
        if (frame) {
            if (!initialized_ || frame->beat.count < lastCount_) {
                lastCount_ = frame->beat.count;  // erster Blick (oder Quelle neu gestartet): nichts nachholen
                initialized_ = true;
            }
            if (frame->beat.count > lastCount_) {
                lastCount_ = frame->beat.count;
                beatNow = frame->beat.strength >= in.number("threshold", 1.0);
            }
            bpm = in.number("manualBpm", 0.0) > 0.0 ? in.number("manualBpm", 0.0) : frame->beat.bpm;
        } else {
            initialized_ = false;
        }
        clock_.emit(context, beatNow, context.time.time.count(), bpm, in.number("decay", 0.25), outputs);
        outputs.set("present", frame.has_value());
    }

private:
    std::shared_ptr<AudioStore> store_;
    BeatClock clock_;
    bool initialized_{false};
    std::uint64_t lastCount_{0};
};

// --- Einsatz (Kick / Snare / Hi-Hat) ------------------------------------------------------------

/// Löst bei jedem Einsatz im gewählten Frequenzbereich aus (Kick, Snare, Hi-Hat oder ein eigener Bereich in Hz), mit
/// klingendem Puls. Die drei Instrumente rechnet der Analysator vor; für den eigenen Bereich führt der Node einen
/// eigenen Erkenner, den er mit jedem neuen Audio-Frame füttert (er sieht damit so viele Spektren, wie der Node
/// ausgewertet wird, also etwas weniger als der Analysator).
class AudioOnsetKernel final : public values::ICpuKernel {
public:
    explicit AudioOnsetKernel(std::shared_ptr<AudioStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const auto frame = freshFrame(*store_);
        const double now = context.time.time.count();
        const auto band = static_cast<int>(std::lround(in.number("band", 0.0)));
        const bool custom = band == 3;
        std::pair<std::size_t, std::size_t> range{0, 0};
        if (custom) {
            const double a = in.number("from", 100.0);
            const double b = in.number("to", 400.0);
            range = bandRange(std::min(a, b), std::max(a, b));
        }

        bool hit = false;
        if (frame) {
            if (custom) {
                feedCustom(*frame, range, context.time.delta.count());
            }
            const audio::OnsetInfo& onset =
                custom ? custom_->info() : (band == 1 ? frame->snare : (band == 2 ? frame->hat : frame->kick));
            if (!initialized_ || onset.count < lastCount_ || band != lastBand_) {
                lastCount_ = onset.count;  // erster Blick, Quelle neu gestartet oder Bereich gewechselt: nichts nachholen
                lastBand_ = band;
                initialized_ = true;
            }
            if (onset.count > lastCount_) {
                lastCount_ = onset.count;
                if (onset.strength >= in.number("threshold", 1.0)) {
                    hit = true;
                    lastBeat_ = now;
                    // 1 = gerade über der Schwelle, 4 und mehr = sehr deutlich
                    accent_ = std::clamp((static_cast<double>(onset.strength) - 1.0) / 3.0, 0.0, 1.0) * 0.5 + 0.5;
                }
            }
        } else {
            initialized_ = false;
            custom_.reset();
        }
        MonitorView view;
        if (frame) {
            view.bands = frame->spectrum;
        }
        const bool hat = band == 2;
        if (custom) {
            view.rangeFirst = static_cast<int>(range.first);
            view.rangeLast = static_cast<int>(range.second);
        } else {
            view.rangeFirst = static_cast<int>(band == 1 ? audio::kSnareFirstBand : (hat ? audio::kHatFirstBand : audio::kKickFirstBand));
            view.rangeLast = static_cast<int>((band == 1 ? audio::kSnareEndBand : (hat ? kSpectrumBands : audio::kKickEndBand)) - 1);
        }
        outputs.setMonitor(view);
        const double decay = in.number("decay", 0.2);
        const double elapsed = std::max(0.0, now - lastBeat_);
        const double pulse = decay > 1e-6 ? std::exp(-elapsed / decay) : (hit ? 1.0 : 0.0);
        outputs.set("beat", hit);
        outputs.set("pulse", pulse);
        outputs.set("strength", pulse * accent_);
        outputs.set("age", std::min(elapsed, 60.0));
        outputs.set("present", frame.has_value());
    }

private:
    /// Füttert den eigenen Erkenner; ein neuer Bereich beginnt mit leerem Erkenner (und Zählerstand 0).
    void feedCustom(const audio::AudioFrame& frame, std::pair<std::size_t, std::size_t> range, double delta) {
        if (!custom_ || range != customRange_) {
            const double hop = delta > 0.001 ? delta : 0.02;  // so oft sieht der Erkenner ein neues Spektrum
            custom_.emplace(hop, range.first, range.second + 1, kCustomMinInterval);
            customRange_ = range;
            lastFrameCount_ = frame.frameCount + 1;  // das aktuelle Spektrum zählt als neu
        }
        if (frame.frameCount != lastFrameCount_) {
            custom_->push(frame.spectrum, frame.audioTime);
            lastFrameCount_ = frame.frameCount;
        }
    }

    static constexpr double kCustomMinInterval = 0.1;  // kürzester Abstand zweier Treffer im eigenen Bereich (s)

    std::shared_ptr<AudioStore> store_;
    bool initialized_{false};
    std::uint64_t lastCount_{0};
    int lastBand_{0};
    std::optional<audio::OnsetDetector> custom_;
    std::pair<std::size_t, std::size_t> customRange_{0, 0};
    std::uint64_t lastFrameCount_{0};
    double lastBeat_{-1e9};
    double accent_{1.0};
};

// --- Tap-Tempo --------------------------------------------------------------------------------

class TapTempoKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double now = context.time.time.count();

        const bool tap = in.boolean("tap", false);
        const bool tapped = tap && !wasTapping_;  // steigende Flanke zählt
        wasTapping_ = tap;
        if (tapped) {
            if (haveTap_ && now - lastTap_ <= 2.0) {
                intervals_.push_back(now - lastTap_);
                if (intervals_.size() > 4) {
                    intervals_.erase(intervals_.begin());
                }
                double sum = 0.0;
                for (const double v : intervals_) {
                    sum += v;
                }
                tapBpm_ = std::clamp(60.0 / (sum / static_cast<double>(intervals_.size())), 30.0, 300.0);
            } else {
                intervals_.clear();  // zu lange Pause: neue Folge
            }
            lastTap_ = now;
            haveTap_ = true;
            periodsDone_ = 0.0;
        }

        const double manual = in.number("manualBpm", 0.0);
        const double bpm = manual > 0.0 ? manual : tapBpm_;
        bool beatNow = tapped;
        double beatTime = now;
        if (!tapped && haveTap_ && bpm > 0.0) {
            // Nach dem letzten Tipp läuft der Takt weiter: ein Schlag je ganzer Periode.
            const double periods = std::floor((now - lastTap_) * bpm / 60.0);
            if (periods > periodsDone_) {
                beatNow = true;
                periodsDone_ = periods;
                beatTime = lastTap_ + periods * 60.0 / bpm;
            }
        }
        clock_.emit(context, beatNow, beatTime, bpm, in.number("decay", 0.25), outputs);
    }

private:
    BeatClock clock_;
    bool wasTapping_{false};
    bool haveTap_{false};
    double lastTap_{0.0};
    double tapBpm_{0.0};
    double periodsDone_{0.0};
    std::vector<double> intervals_;
};

const std::vector<std::string> kLevelScale{"Linear", "dB"};
const std::vector<std::string> kOnsetBand{"Kick", "Snare", "Hi-Hat", "Custom range (From / To)"};

std::vector<graph::ParameterDescriptor> followParameters(double release) {
    return {number("gain", "Gain", 1.0, 0.0, 50.0), number("attack", "Attack (s)", 0.01, 0.0, 5.0),
            number("release", "Release (s)", release, 0.0, 10.0)};
}

}  // namespace

std::vector<values::CpuNodeDefinition> audioNodes(std::shared_ptr<audio::AudioStore> store) {
    using graph::PortType;
    if (!store) {
        store = std::make_shared<AudioStore>();  // ohne Quelle: dauerhaft "kein Signal"
    }
    std::vector<values::CpuNodeDefinition> nodes;

    auto levelParameters = followParameters(0.25);
    levelParameters.push_back(choice("scale", "Scale", kLevelScale, 1));
    levelParameters.push_back(toggle("auto", "Auto gain", false));
    nodes.push_back(cpuNode("wh.value.audiolevel", "Audio Level", "Audio",
                            {valueOutput("level", "Level", PortType::Scalar), valueOutput("peak", "Peak", PortType::Scalar),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            std::move(levelParameters), [store] { return std::make_unique<AudioLevelKernel>(store); }));

    auto bandParameters = followParameters(0.2);
    bandParameters.insert(bandParameters.begin(), number("to", "To (Hz)", 250.0, 40.0, 16000.0));
    bandParameters.insert(bandParameters.begin(), number("from", "From (Hz)", 60.0, 40.0, 16000.0));
    bandParameters.push_back(number("floor", "Noise floor", 0.0, 0.0, 0.95));
    nodes.push_back(cpuNode("wh.value.audioband", "Audio Band", "Audio",
                            {valueOutput("value", "Value", PortType::Scalar), valueOutput("present", "Signal", PortType::Boolean)},
                            std::move(bandParameters), [store] { return std::make_unique<AudioBandKernel>(store); }));
    nodes.back().descriptor.liveView = graph::LiveView::Spectrum;

    auto spectrumParameters = followParameters(0.2);
    spectrumParameters.push_back(number("floor", "Noise floor", 0.0, 0.0, 0.95));
    nodes.push_back(cpuNode("wh.value.audiospectrum", "Audio Spectrum", "Audio",
                            {valueOutput("spectrum", "Spectrum", PortType::AudioSpectrum),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            std::move(spectrumParameters), [store] { return std::make_unique<AudioSpectrumKernel>(store); }));
    nodes.back().descriptor.liveView = graph::LiveView::Spectrum;

    nodes.push_back(cpuNode("wh.value.beat", "Beat", "Audio",
                            {valueOutput("beat", "Beat", PortType::Trigger), valueOutput("pulse", "Pulse", PortType::Scalar),
                             valueOutput("phase", "Phase", PortType::Scalar), valueOutput("bpm", "BPM", PortType::Scalar),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            {number("threshold", "Min. strength", 1.0, 1.0, 6.0), number("decay", "Pulse decay (s)", 0.25, 0.0, 5.0),
                             number("manualBpm", "Fixed tempo (BPM, 0 = detect)", 0.0, 0.0, 300.0)},
                            [store] { return std::make_unique<AudioBeatKernel>(store); }));

    nodes.push_back(cpuNode("wh.value.onset", "Audio Trigger", "Audio",
                            {valueOutput("beat", "Hit", PortType::Trigger), valueOutput("pulse", "Pulse", PortType::Scalar),
                             valueOutput("strength", "Strength", PortType::Scalar), valueOutput("age", "Age (s)", PortType::Scalar),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            {choice("band", "Instrument", kOnsetBand, 0), number("from", "From (Hz, custom range)", 100.0, 40.0, 16000.0),
                             number("to", "To (Hz, custom range)", 400.0, 40.0, 16000.0), number("threshold", "Min. strength", 1.0, 1.0, 6.0),
                             number("decay", "Pulse decay (s)", 0.2, 0.0, 5.0)},
                            [store] { return std::make_unique<AudioOnsetKernel>(store); }));
    nodes.back().descriptor.liveView = graph::LiveView::Spectrum;

    nodes.push_back(cpuNode("wh.value.tap", "Tap Tempo", "Audio",
                            {valueOutput("beat", "Beat", PortType::Trigger), valueOutput("pulse", "Pulse", PortType::Scalar),
                             valueOutput("phase", "Phase", PortType::Scalar), valueOutput("bpm", "BPM", PortType::Scalar)},
                            {toggle("tap", "Tap (each switch on counts)", false), number("manualBpm", "Fixed tempo (BPM, 0 = from taps)", 0.0, 0.0, 300.0),
                             number("decay", "Pulse decay (s)", 0.25, 0.0, 5.0)},
                            kernelFactory<TapTempoKernel>()));
    return nodes;
}

}  // namespace whitehole::nodes::detail
