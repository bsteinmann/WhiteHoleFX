// Zeitliche Werte-Nodes: Clock, Envelope und Sequencer.
//
// Warum es diese Datei gibt: Bisher hängt alles Zeitliche an LFOs oder am Audio-Beat. Für Shows,
// die sich wie ein kleines Instrument spielen lassen, fehlen drei Bausteine:
//   - Clock: ein Taktgeber (BPM mal Verhältnis) mit Tick, Phase und Zähler, einstellbar oder von
//     Tap Tempo gespeist;
//   - Envelope: eine ADSR-Hüllkurve, die ein Signal (Gate/Trigger) in einen weichen Verlauf
//     verwandelt, zum Beispiel einen Beat in ein Aufleuchten mit Ausklingen;
//   - Sequencer: acht Schritte mit je einem Wert, weitergeschaltet durch ein Signal (meist der
//     Tick der Clock).
// Die Kernel haben Zustand (Phase, Hüllkurvenstufe, Schrittzähler), aber keine Systemuhr: Sie
// rechnen nur mit dem Zeitschritt des Frames und sind darum in Tests genau nachvollziehbar.
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

// --- Clock ------------------------------------------------------------------------------------

constexpr std::array<double, 7> kClockRatios{0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0};

// Die Phase wächst mit bpm / 60 * Verhältnis je Sekunde und wird aufsummiert, nicht aus der
// absoluten Zeit berechnet: Ändert sich das Tempo (Tap Tempo, Modulation), läuft die Uhr weich
// weiter, statt zu springen. Ein Tick ist der Übergang in die nächste ganze Periode; pro Frame
// gibt es höchstens einen, `count` zählt aber alle mit.
class ClockKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        resetEdge_.update(in.boolean("reset", false));
        if (resetEdge_.rose()) {
            position_ = 0.0;
        }
        bool tick = false;
        if (in.boolean("running", true) && !resetEdge_.rose()) {
            const double before = std::floor(position_);
            const std::size_t index = static_cast<std::size_t>(std::clamp<std::int64_t>(in.integer("ratio", 3), 0, 6));
            const double rate = std::max(in.number("bpm", 120.0), 0.0) / 60.0 * kClockRatios[index];
            position_ += std::max(context.time.delta.count(), 0.0) * rate;
            tick = std::floor(position_) > before;
        }
        outputs.set("tick", tick);
        outputs.set("phase", position_ - std::floor(position_));
        outputs.set("count", static_cast<std::int64_t>(std::floor(position_)));
    }

private:
    double position_{0.0};
    EdgeDetector resetEdge_;
};

// --- Envelope ---------------------------------------------------------------------------------

enum class Stage { Idle, Attack, Decay, Sustain, Release };

// ADSR mit linearen Flanken. Eine steigende Flanke am Gate startet den Anschlag vom aktuellen Pegel
// aus (ohne Sprung nach 0); eine fallende Flanke das Ausklingen vom aktuellen Pegel aus. Im Modus
// "One-shot" läuft nach dem Anschlag sofort das Ausklingen, Decay und Sustain entfallen: so wird aus
// einem einzelnen Trigger (ein Frame lang) ein Aufleuchten mit Ausklingen.
class EnvelopeKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const bool gate = in.boolean("gate", false);
        gateEdge_.update(gate);
        const bool oneShot = in.integer("mode", 0) == 1;
        const double sustain = std::clamp(in.number("sustain", 0.6), 0.0, 1.0);
        const double dt = std::max(context.time.delta.count(), 0.0);

        if (gateEdge_.rose()) {
            stage_ = Stage::Attack;
        } else if (gateEdge_.fell() && !oneShot && stage_ != Stage::Idle) {
            beginRelease(in.number("release", 0.5));
        }

        switch (stage_) {
            case Stage::Attack:
                level_ = climb(level_, 1.0, in.number("attack", 0.05), dt);
                if (level_ >= 1.0) {
                    if (oneShot) {
                        beginRelease(in.number("release", 0.5));
                    } else {
                        stage_ = Stage::Decay;
                    }
                }
                break;
            case Stage::Decay:
                level_ = climb(level_, sustain, in.number("decay", 0.2), dt);
                if (level_ <= sustain) {
                    stage_ = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                level_ = sustain;
                break;
            case Stage::Release:
                level_ = std::max(level_ - releaseSlope_ * dt, 0.0);
                if (level_ <= 0.0) {
                    stage_ = Stage::Idle;
                }
                break;
            case Stage::Idle:
                level_ = 0.0;
                break;
        }
        outputs.set("out", level_);
        outputs.set("active", stage_ != Stage::Idle);
    }

private:
    // Bewegt den Pegel mit der Geschwindigkeit "Weg / Zeit" auf das Ziel zu und über es nicht hinaus.
    // Weg ist hier immer der Abstand von 0 nach 1 (Anschlag) bzw. von 1 auf das Ziel (Decay): die Zeit
    // gilt für den vollen Hub, so bleibt sie bei einem Neustart aus mittlerem Pegel kürzer.
    static double climb(double level, double target, double seconds, double dt) {
        if (seconds <= 1e-9) {
            return target;
        }
        const double step = dt / seconds;
        return level < target ? std::min(level + step, target) : std::max(level - step, target);
    }

    void beginRelease(double seconds) {
        stage_ = Stage::Release;
        // Zeit für den vollen Hub von 1 auf 0: aus halber Höhe dauert es die Hälfte.
        releaseSlope_ = seconds <= 1e-9 ? 1e9 : 1.0 / seconds;
    }

    Stage stage_{Stage::Idle};
    double level_{0.0};
    double releaseSlope_{2.0};
    EdgeDetector gateEdge_{true};
};

// --- Sequencer --------------------------------------------------------------------------------

constexpr int kMaxSteps = 8;
constexpr std::array<const char*, kMaxSteps> kStepKeys{"step1", "step2", "step3", "step4",
                                                      "step5", "step6", "step7", "step8"};
constexpr std::array<double, kMaxSteps> kStepDefaults{1.0, 0.0, 0.5, 0.0, 1.0, 0.0, 0.5, 0.0};

// Hält einen Schrittzähler, der bei jeder steigenden Flanke an `advance` um eins wächst; welcher
// der Schritte gerade gilt, ergibt sich aus Zähler, Schrittzahl und Richtung. Weil die Position
// jedes Mal neu berechnet wird, bleibt der Ablauf sinnvoll, wenn man `steps` oder die Richtung
// im Betrieb ändert.
class SequencerKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        advanceEdge_.update(in.boolean("advance", false));
        resetEdge_.update(in.boolean("reset", false));
        const std::int64_t steps = std::clamp<std::int64_t>(in.integer("steps", 8), 1, kMaxSteps);
        const std::int64_t direction = in.integer("direction", 0);

        bool moved = false;
        if (resetEdge_.rose()) {
            counter_ = 0;
            moved = true;
        } else if (advanceEdge_.rose()) {
            ++counter_;
            moved = true;
        }
        const std::int64_t index = position(counter_, steps, direction, in.integer("seed", 0));
        const double value = in.number(kStepKeys[static_cast<std::size_t>(index)], 0.0);

        outputs.set("value", value);
        outputs.set("step", index);
        outputs.set("trigger", moved && !resetEdge_.rose());
        outputs.set("hit", moved && !resetEdge_.rose() && value > 0.0);
    }

private:
    // 0 vorwärts, 1 rückwärts, 2 hin und her (ohne die Endschritte doppelt zu spielen), 3 zufällig.
    static std::int64_t position(std::int64_t counter, std::int64_t steps, std::int64_t direction,
                                 std::int64_t seed) {
        switch (direction) {
            case 1:
                return steps - 1 - counter % steps;
            case 2: {
                const std::int64_t period = std::max<std::int64_t>(2 * steps - 2, 1);
                const std::int64_t p = counter % period;
                return p < steps ? p : period - p;
            }
            case 3:
                return counter == 0 ? 0
                                    : std::min<std::int64_t>(
                                          steps - 1, static_cast<std::int64_t>(hashToUnit(counter, seed) * static_cast<double>(steps)));
            default:
                return counter % steps;
        }
    }

    std::int64_t counter_{0};
    EdgeDetector advanceEdge_;
    EdgeDetector resetEdge_;
};

}  // namespace

std::vector<values::CpuNodeDefinition> sequenceNodes() {
    using graph::PortType;
    std::vector<values::CpuNodeDefinition> nodes;

    nodes.push_back(cpuNode(
        "wh.value.clock", "Clock", "Value",
        {valueOutput("tick", "Tick", PortType::Trigger), valueOutput("phase", "Phase", PortType::Scalar),
         valueOutput("count", "Count", PortType::Integer)},
        {number("bpm", "Tempo (BPM)", 120.0, 1.0, 400.0),
         choice("ratio", "Rate", {"1/8 (every 8 beats)", "1/4 (every 4 beats)", "1/2 (every 2 beats)", "1x (every beat)",
                                  "2x", "4x", "8x"}, 3),
         toggle("running", "Running", true), toggle("reset", "Reset (each switch on restarts)", false)},
        kernelFactory<ClockKernel>()));

    nodes.push_back(cpuNode(
        "wh.value.envelope", "Envelope", "Value",
        {valueOutput("out", "Out", PortType::Scalar), valueOutput("active", "Active", PortType::Boolean)},
        {toggle("gate", "Gate", false), choice("mode", "Mode", {"ADSR (follows gate)", "One-shot (attack, release)"}),
         number("attack", "Attack (s)", 0.05, 0.0, 30.0), number("decay", "Decay (s)", 0.2, 0.0, 30.0),
         number("sustain", "Sustain", 0.6, 0.0, 1.0), number("release", "Release (s)", 0.5, 0.0, 30.0)},
        kernelFactory<EnvelopeKernel>()));

    std::vector<graph::ParameterDescriptor> parameters{
        toggle("advance", "Advance (each switch on steps)", false),
        toggle("reset", "Reset (each switch on goes to step 1)", false),
        integer("steps", "Steps", 8, 1.0, static_cast<double>(kMaxSteps)),
        choice("direction", "Direction", {"Forward", "Backward", "Ping-pong", "Random"}),
        integer("seed", "Random seed", 0)};
    for (int i = 0; i < kMaxSteps; ++i) {
        parameters.push_back(number(kStepKeys[static_cast<std::size_t>(i)], "Step " + std::to_string(i + 1),
                                    kStepDefaults[static_cast<std::size_t>(i)], 0.0, 1.0));
    }
    nodes.push_back(cpuNode(
        "wh.value.sequencer", "Sequencer", "Value",
        {valueOutput("value", "Value", PortType::Scalar), valueOutput("step", "Step", PortType::Integer),
         valueOutput("trigger", "Trigger", PortType::Trigger), valueOutput("hit", "Hit", PortType::Trigger)},
        std::move(parameters), kernelFactory<SequencerKernel>()));
    return nodes;
}

}  // namespace whitehole::nodes::detail
