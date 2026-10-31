// Werte-Nodes: erzeugen und formen Zahlen und Farben, die Parameter anderer
// Nodes steuern (Modulation). Sie laufen auf der CPU, einmal pro Frame.
//
// Alle Kernel sind frei von Systemuhr und globalem Zustand: Zeit kommt aus dem
// FrameTime, Zufall aus einer Hashfunktion. Derselbe Graph liefert damit bei
// derselben Zeit immer dieselben Werte, auch beim Offline-Rendern und in Tests.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

double fract(double value) { return value - std::floor(value); }

// Ausgang = Zeit * Tempo + Versatz. Tempo darf negativ sein (Rückwärtslauf).
class TimeKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        outputs.set("seconds", context.time.time.count() * context.inputs.number("speed", 1.0) +
                                   context.inputs.number("offset", 0.0));
    }
};

class NumberKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        outputs.set("out", context.inputs.number("value", 0.0));
    }
};

class ColorKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        outputs.set("out", adjustColor(in.color("color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}), in.number("hue", 0.0),
                                       in.number("saturation", 1.0), in.number("brightness", 1.0)));
    }
};

// Schwingung in [0,1] aus der Phase u in [0,1):
//   0 Sinus (startet bei 0.5 steigend), 1 Dreieck (startet bei 0), 2 Sägezahn, 3 Rechteck.
double wave(std::int64_t shape, double u) {
    switch (shape) {
        case 1:
            return 1.0 - std::abs(2.0 * u - 1.0);
        case 2:
            return u;
        case 3:
            return u < 0.5 ? 1.0 : 0.0;
        default:
            return 0.5 + 0.5 * std::sin(2.0 * std::numbers::pi * u);
    }
}

// Zustandslos: Die Phase folgt der absoluten Zeit. Das macht Springen im Zeitverlauf
// (Offline-Rendern, Scrubben) exakt, hat aber einen Preis: Ändert sich die Frequenz
// während des Laufs, springt die Phase. Ein akkumulierender Oszillator wäre die
// Alternative, wenn Frequenzmodulation gebraucht wird.
class LfoKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double u =
            fract(context.time.time.count() * in.number("frequency", 0.5) + in.number("phase", 0.0));
        const double low = in.number("min", 0.0);
        const double high = in.number("max", 1.0);
        outputs.set("out", low + (high - low) * wave(in.integer("shape", 0), u));
    }
};

// SplitMix64-artiger Hash auf [0,1): plattformunabhängig und ohne <random>.
double hashToUnit(std::int64_t index, std::int64_t seed) {
    std::uint64_t x = static_cast<std::uint64_t>(index) * 0x9E3779B97F4A7C15ULL +
                      static_cast<std::uint64_t>(seed) * 0xBF58476D1CE4E5B9ULL + 0x94D049BB133111EBULL;
    x ^= x >> 30U;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27U;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31U;
    return static_cast<double>(x >> 11U) * (1.0 / 9007199254740992.0);  // 2^53
}

// Neuer Zufallswert `rate`-mal pro Sekunde; dazwischen weich überblendet oder gehalten.
class RandomKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double position = context.time.time.count() * in.number("rate", 1.0);
        const double cell = std::floor(position);
        const double f = position - cell;
        const auto index = static_cast<std::int64_t>(cell);
        const std::int64_t seed = in.integer("seed", 0);

        const double a = hashToUnit(index, seed);
        if (!in.boolean("smooth", true)) {
            outputs.set("out", a);
            return;
        }
        const double b = hashToUnit(index + 1, seed);
        outputs.set("out", a + (b - a) * (f * f * (3.0 - 2.0 * f)));
    }
};

// Rechnet einen Bereich in einen anderen um. Ein leerer Eingangsbereich liefert outMin.
class MapKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const double inMin = in.number("inMin", 0.0);
        const double span = in.number("inMax", 1.0) - inMin;
        double t = std::abs(span) < 1e-12 ? 0.0 : (in.number("value", 0.0) - inMin) / span;
        if (in.boolean("clamp", true)) {
            t = std::clamp(t, 0.0, 1.0);
        }
        const double outMin = in.number("outMin", 0.0);
        outputs.set("out", outMin + t * (in.number("outMax", 1.0) - outMin));
    }
};

// Glättet einen Wert mit einem Tiefpass erster Ordnung. Dies ist der Zustand, der
// über Frames hinweg lebt. `time` ist die Zeitkonstante: Nach dieser Zeit ist etwa
// 63 % des Sprungs erreicht. Beim ersten Frame gibt es keinen Anlauf von null.
class SmoothKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const double target = context.inputs.number("value", 0.0);
        const double tau = context.inputs.number("time", 0.2);
        if (!initialized_ || tau <= 1e-9) {
            state_ = target;
            initialized_ = true;
        } else {
            const double alpha = 1.0 - std::exp(-context.time.delta.count() / tau);
            state_ += (target - state_) * alpha;
        }
        outputs.set("out", state_);
    }

private:
    bool initialized_{false};
    double state_{0.0};
};

}  // namespace

std::vector<values::CpuNodeDefinition> valueNodes() {
    using graph::PortType;
    std::vector<values::CpuNodeDefinition> nodes;

    nodes.push_back(cpuNode("wh.value.time", "Time", "Value",
                            {valueOutput("seconds", "Seconds", PortType::Scalar)},
                            {number("speed", "Speed", 1.0, -1000.0, 1000.0),
                             number("offset", "Offset (s)", 0.0),
                             number("speedMin", "Speed slider min", -4.0),
                             number("speedMax", "Speed slider max", 4.0)},
                            kernelFactory<TimeKernel>()));

    nodes.push_back(cpuNode("wh.value.number", "Number", "Value",
                            {valueOutput("out", "Out", PortType::Scalar)},
                            {number("value", "Value", 0.0)}, kernelFactory<NumberKernel>()));

    nodes.push_back(cpuNode("wh.value.color", "Color", "Value",
                            {valueOutput("out", "Out", PortType::Color)},
                            {color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
                             number("hue", "Hue shift (turns)", 0.0), number("saturation", "Saturation", 1.0, 0.0, 1.0),
                             number("brightness", "Brightness", 1.0, 0.0, 1.0)},
                            kernelFactory<ColorKernel>()));
    nodes.push_back(colorRampNode());

    nodes.push_back(cpuNode(
        "wh.value.lfo", "LFO", "Value", {valueOutput("out", "Out", PortType::Scalar)},
        {choice("shape", "Shape", {"Sine", "Triangle", "Saw", "Square"}),
         number("frequency", "Frequency (Hz)", 0.5, 0.0, 100.0), number("phase", "Phase", 0.0, 0.0, 1.0),
         number("min", "Min", 0.0), number("max", "Max", 1.0),
         number("frequencyMin", "Frequency slider min", 0.0), number("frequencyMax", "Frequency slider max", 5.0)},
        kernelFactory<LfoKernel>()));

    nodes.push_back(cpuNode("wh.value.random", "Random", "Value",
                            {valueOutput("out", "Out", PortType::Scalar)},
                            {number("rate", "Rate (Hz)", 1.0, 0.01, 1000.0), integer("seed", "Seed", 0),
                             toggle("smooth", "Smooth", true)},
                            kernelFactory<RandomKernel>()));

    nodes.push_back(cpuNode("wh.value.map", "Map", "Value",
                            {valueOutput("out", "Out", PortType::Scalar)},
                            {number("value", "Value", 0.0), number("inMin", "In min", 0.0),
                             number("inMax", "In max", 1.0), number("outMin", "Out min", 0.0),
                             number("outMax", "Out max", 1.0), toggle("clamp", "Clamp", true)},
                            kernelFactory<MapKernel>()));

    nodes.push_back(cpuNode("wh.value.smooth", "Smooth", "Value",
                            {valueOutput("out", "Out", PortType::Scalar)},
                            {number("value", "Value", 0.0), number("time", "Time constant (s)", 0.2, 0.0, 60.0)},
                            kernelFactory<SmoothKernel>()));

    return nodes;
}

}  // namespace whitehole::nodes::detail
