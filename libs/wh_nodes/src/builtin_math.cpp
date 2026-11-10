// Rechen- und Vergleichs-Nodes: Math und Compare.
//
// Warum es diese Datei gibt: Mit LFO, Random, Map und Smooth lassen sich Werte erzeugen und umformen,
// aber nicht miteinander verrechnen (zwei Pegel addieren, einen Wert quantisieren) oder in eine
// Ja/Nein-Entscheidung verwandeln. Math ist der Taschenrechner dafür; Compare macht aus einer Zahl
// einen Schalter (mit Hysterese, damit ein flackernder Pegel nicht ständig umschaltet) und meldet
// die Flanken als Trigger. Beides ist reine CPU-Arbeit ohne Systemuhr: gleiche Eingaben, gleiche Ausgabe.
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

enum MathOp : std::int64_t { Add, Subtract, Multiply, Divide, Min, Max, Power, Modulo, Abs, Quantize, Sine };

/// Das Ergebnis von `a op b`. Alles, was keine Zahl wäre (Teilen durch 0, Wurzel aus Negativem,
/// Überlauf), wird zu 0, damit ein einzelner Fehler nicht den ganzen Graphen mit NaN anfüllt.
double compute(std::int64_t op, double a, double b) {
    double result = 0.0;
    switch (op) {
        case Add: result = a + b; break;
        case Subtract: result = a - b; break;
        case Multiply: result = a * b; break;
        case Divide: result = b == 0.0 ? 0.0 : a / b; break;
        case Min: result = std::min(a, b); break;
        case Max: result = std::max(a, b); break;
        case Power: result = std::pow(a, b); break;
        case Modulo: result = b == 0.0 ? 0.0 : a - b * std::floor(a / b); break;  // Ergebnis hat das Vorzeichen von b
        case Abs: result = std::abs(a); break;
        case Quantize: result = b == 0.0 ? a : std::round(a / b) * b; break;
        case Sine: result = std::sin(2.0 * std::numbers::pi * a) * b; break;  // a in Umdrehungen, b = Höhe
        default: break;
    }
    return std::isfinite(result) ? result : 0.0;
}

class MathKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        outputs.set("out", compute(in.integer("op", Add), in.number("a", 0.0), in.number("b", 0.0)));
    }
};

// Schwellwert mit Hysterese: "an", sobald der Wert über threshold + hysteresis/2 steigt, und erst
// wieder "aus" unter threshold - hysteresis/2. Im ersten Frame gilt der einfache Vergleich mit der
// Schwelle (ohne Hysterese) und es gibt keine Flanke.
// Die Modi Between (zwischen threshold und upper) und Equal (threshold ± tolerance) prüfen einen Bereich
// [lo, hi]; die Hysterese macht ihn zum Einschalten enger und zum Ausschalten weiter (je hysteresis/2 an
// jeder Kante), damit ein Wert an der Grenze nicht flackert.
class CompareKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double value = in.number("value", 0.0);
        const double threshold = in.number("threshold", 0.5);
        const double halfBand = std::max(in.number("hysteresis", 0.0), 0.0) * 0.5;
        const std::int64_t mode = in.integer("mode", 0);
        if (mode != mode_) {
            mode_ = mode;
            initialized_ = false;  // anderer Vergleich: der erste Blick gilt ohne Hysterese
        }
        bool state = false;
        if (mode >= 2) {
            double lo = threshold;
            double hi = in.number("upper", 1.0);
            if (mode == 2) {
                if (lo > hi) {
                    std::swap(lo, hi);
                }
            } else {
                const double tolerance = std::max(in.number("tolerance", 0.05), 0.0);
                lo = threshold - tolerance;
                hi = threshold + tolerance;
            }
            if (!initialized_) {
                inside_ = value >= lo && value <= hi;
            } else if (inside_ && (value < lo - halfBand || value > hi + halfBand)) {
                inside_ = false;
            } else if (!inside_ && value >= lo + halfBand && value <= hi - halfBand) {
                inside_ = true;
            }
            state = inside_;
        } else {
            if (!initialized_) {
                above_ = value > threshold;
            } else if (above_ && value < threshold - halfBand) {
                above_ = false;
            } else if (!above_ && value > threshold + halfBand) {
                above_ = true;
            }
            state = mode == 1 ? !above_ : above_;
        }
        edge_.update(state);
        initialized_ = true;
        outputs.set("out", state);
        outputs.set("rise", edge_.rose());
        outputs.set("fall", edge_.fell());
    }

private:
    bool initialized_{false};
    bool above_{false};
    bool inside_{false};
    std::int64_t mode_{0};
    EdgeDetector edge_;
};

}  // namespace

std::vector<values::CpuNodeDefinition> mathNodes() {
    using graph::PortType;
    std::vector<values::CpuNodeDefinition> nodes;
    nodes.push_back(cpuNode(
        "wh.value.math", "Math", "Value", {valueOutput("out", "Out", PortType::Scalar)},
        {choice("op", "Operation",
                {"Add", "Subtract", "Multiply", "Divide", "Min", "Max", "Power", "Modulo", "Absolute of A",
                 "Quantize A to steps of B", "Sine of A turns times B"}),
         number("a", "A", 0.0), number("b", "B", 1.0)},
        kernelFactory<MathKernel>()));
    nodes.push_back(cpuNode(
        "wh.value.compare", "Compare", "Value",
        {valueOutput("out", "Out", PortType::Boolean), valueOutput("rise", "Rise", PortType::Trigger),
         valueOutput("fall", "Fall", PortType::Trigger)},
        {number("value", "Value", 0.0), number("threshold", "Threshold", 0.5),
         number("hysteresis", "Hysteresis", 0.0, 0.0, 10.0),
         choice("mode", "On when value is",
                {"Above", "Below", "Between Threshold and Upper", "Equal to Threshold (within Tolerance)"}),
         number("upper", "Upper limit (Between)", 1.0), number("tolerance", "Tolerance (Equal)", 0.05, 0.0, 10.0)},
        kernelFactory<CompareKernel>()));
    return nodes;
}

}  // namespace whitehole::nodes::detail
