// Shot: ein Trigger startet einen Durchlauf von 0 nach 1, dessen Dauer am Tempo der Musik hängt.
//
// Warum es diese Datei gibt: Viele Effekte brauchen "bei jedem Schlag einmal etwas ablaufen lassen":
// ein Balken, der über die Matrix wischt, eine Form, die sich ausdehnt, ein Aufblitzen, das ausklingt.
// Die Envelope kann das nicht (sie steigt und fällt wieder, ihre Dauer ist eine feste Zeit), der LFO läuft
// ohne Trigger. Der Shot-Node liefert den Fortschritt 0..1 eines Durchlaufs; per Map-Node lässt er sich
// auf jeden Regler legen (Radius, Position, Helligkeit ...). So bleibt der Node unabhängig davon, was
// animiert wird.
//
// Verhalten:
//   - Ein Trigger (steigende Flanke an `trigger`) startet `repeats` Durchläufe hintereinander. Kommt ein
//     neuer Trigger während des Laufs, beginnt alles von vorn.
//   - Ein Durchlauf dauert `beats` Schläge. Die Länge eines Schlags kommt aus `bpm`, sonst aus dem Abstand der
//     letzten Trigger (geglättet), sonst aus `beatLength` (nur bis zum zweiten Trigger).
//   - Die Phase wird aufsummiert, nicht aus der Zeit seit dem Trigger berechnet: Ändert sich das Tempo
//     mitten im Lauf, wird der Rest weich schneller oder langsamer statt zu springen.
//   - Nach dem letzten Durchlauf bleibt `progress` am Ende stehen; `active` zeigt, ob noch etwas läuft.
// Keine Systemuhr: Es zählt nur der Zeitschritt des Frames, damit Tests und Offline-Rendering genau stimmen.
#include <algorithm>
#include <cmath>

#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

constexpr double kMinInterval = 0.12;  // kürzere Abstände zwischen Triggern zählen nicht als Schlag (Prellen)
constexpr double kMaxInterval = 4.0;   // längere Pausen auch nicht (15 BPM): danach beginnt die Messung neu
constexpr double kIntervalSmoothing = 0.6;  // Anteil des neuen Abstands am geglätteten Wert

/// Formt den Fortschritt eines Durchlaufs: 0 linear, 1 langsam am Ende, 2 langsam am Anfang, 3 weich an beiden Enden.
double shape(std::int64_t curve, double p) {
    switch (curve) {
        case 1: return 1.0 - std::pow(1.0 - p, 3.0);
        case 2: return p * p * p;
        case 3: return p * p * (3.0 - 2.0 * p);
        default: return p;
    }
}

class ShotKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double dt = std::max(context.time.delta.count(), 0.0);
        clock_ += dt;
        triggerEdge_.update(in.boolean("trigger", false));

        const double beatSeconds = beatLength(in);
        const double duration = std::max(in.number("beats", 1.0), 0.001) * beatSeconds;
        const std::int64_t repeats = std::clamp<std::int64_t>(in.integer("repeats", 1), 1, kMaxRepeats);

        bool done = false;
        if (triggerEdge_.rose()) {
            measure();
            ++count_;
            phase_ = 0.0;
            running_ = true;
        } else if (running_) {
            phase_ += dt / duration;
            if (phase_ >= static_cast<double>(repeats)) {
                phase_ = static_cast<double>(repeats);
                running_ = false;
                done = true;
            }
        }

        const bool alternate = in.boolean("alternate", false);
        double progress = 0.0;
        std::int64_t run = 0;
        if (count_ > 0) {
            // Am Ende (phase_ == repeats) zählt der letzte Durchlauf als vollständig durchlaufen.
            const double last = static_cast<double>(repeats);
            run = std::min<std::int64_t>(static_cast<std::int64_t>(std::floor(phase_)), repeats - 1);
            const double within = phase_ >= last ? 1.0 : phase_ - std::floor(phase_);
            progress = shape(in.integer("curve", 0), within);
            if (alternate && run % 2 == 1) {
                progress = 1.0 - progress;
            }
        }

        outputs.set("progress", progress);
        outputs.set("reverse", 1.0 - progress);
        outputs.set("active", running_);
        outputs.set("done", done);
        outputs.set("run", count_ > 0 ? run + 1 : std::int64_t{0});
        outputs.set("count", count_);
        outputs.set("flip", count_ % 2 == 1);
    }

private:
    static constexpr std::int64_t kMaxRepeats = 16;

    /// Länge eines Schlags in Sekunden: fester Wert `bpm`, sonst gemessen, sonst die Vorgabe.
    double beatLength(const values::KernelInputs& in) const {
        const double bpm = in.number("bpm", 0.0);
        if (bpm > 0.0) {
            return 60.0 / bpm;
        }
        if (interval_ > 0.0) {
            return interval_;
        }
        return std::max(in.number("beatLength", 0.5), 0.01);
    }

    /// Merkt sich den Abstand zum vorigen Trigger, soweit er wie ein Schlag aussieht.
    void measure() {
        if (lastTrigger_ >= 0.0) {
            const double gap = clock_ - lastTrigger_;
            if (gap >= kMinInterval && gap <= kMaxInterval) {
                interval_ = interval_ > 0.0 ? interval_ + (gap - interval_) * kIntervalSmoothing : gap;
            } else if (gap > kMaxInterval) {
                interval_ = 0.0;  // lange Pause: das Tempo ist wohl ein anderes
            }
        }
        lastTrigger_ = clock_;
    }

    EdgeDetector triggerEdge_;
    double clock_{0.0};
    double lastTrigger_{-1.0};
    double interval_{0.0};
    double phase_{0.0};
    bool running_{false};
    std::int64_t count_{0};
};

}  // namespace

std::vector<values::CpuNodeDefinition> shotNodes() {
    using graph::PortType;
    std::vector<values::CpuNodeDefinition> nodes;
    nodes.push_back(cpuNode(
        "wh.value.shot", "Shot", "Value",
        {valueOutput("progress", "Progress", PortType::Scalar), valueOutput("reverse", "Reverse", PortType::Scalar),
         valueOutput("active", "Active", PortType::Boolean), valueOutput("done", "Done", PortType::Trigger),
         valueOutput("run", "Run", PortType::Integer), valueOutput("count", "Count", PortType::Integer),
         valueOutput("flip", "Alternating", PortType::Boolean)},
        {toggle("trigger", "Trigger (each switch on starts)", false), number("beats", "Length (beats)", 1.0, 0.0625, 64.0),
         integer("repeats", "Repeats", 1, 1.0, 16.0), toggle("alternate", "Every other run backwards", false),
         choice("curve", "Curve", {"Linear", "Slow at the end", "Slow at the start", "Smooth"}),
         number("bpm", "Tempo (BPM, 0 = measure from triggers)", 0.0, 0.0, 400.0),
         number("beatLength", "Beat length until measured (s)", 0.5, 0.05, 5.0)},
        kernelFactory<ShotKernel>()));
    return nodes;
}

}  // namespace whitehole::nodes::detail
