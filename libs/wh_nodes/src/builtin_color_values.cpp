// Farb-Nodes der Werte-Schicht: Farbton, Sättigung und Helligkeit einer Farbe steuern, und
// "Color Ramp", das mit einer Zahl zwischen mehreren Farben überblendet.
//
// Warum es diese Datei gibt: Der Node "Color" lieferte nur eine feste Farbe. Mit diesen Kernen lässt
// sich eine Farbe mit LFO, Random, Time oder Number verändern (Regenbogen, Pulsieren, Blitzen) und
// zwischen Farben wechseln. Wie alle Werte-Kernel sind sie frei von Uhr und Zufall; derselbe Graph
// liefert bei derselben Zeit dieselben Farben. Gerechnet wird mit den Werten "wie sie auf dem
// Bildschirm erscheinen" (siehe color_encoder.hpp), die Ausgabe wendet das Gamma an.
#include <algorithm>
#include <array>
#include <cmath>

#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

struct Hsv {
    float h{0.0F};  // 0..1, wiederholt sich
    float s{0.0F};
    float v{0.0F};
};

Hsv toHsv(const ColorRgba& c) {
    const float maxC = std::max({c.r, c.g, c.b});
    const float minC = std::min({c.r, c.g, c.b});
    const float d = maxC - minC;
    Hsv out;
    out.v = maxC;
    out.s = maxC > 0.0F ? d / maxC : 0.0F;
    if (d > 0.0F) {
        float h = 0.0F;
        if (maxC == c.r) {
            h = std::fmod((c.g - c.b) / d, 6.0F);
        } else if (maxC == c.g) {
            h = (c.b - c.r) / d + 2.0F;
        } else {
            h = (c.r - c.g) / d + 4.0F;
        }
        out.h = h / 6.0F;
        out.h -= std::floor(out.h);
    }
    return out;
}

ColorRgba fromHsv(const Hsv& in, float alpha) {
    const float h = (in.h - std::floor(in.h)) * 6.0F;
    const float c = in.v * in.s;
    const float x = c * (1.0F - std::abs(std::fmod(h, 2.0F) - 1.0F));
    const float m = in.v - c;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    if (h < 1.0F) {
        r = c, g = x;
    } else if (h < 2.0F) {
        r = x, g = c;
    } else if (h < 3.0F) {
        g = c, b = x;
    } else if (h < 4.0F) {
        g = x, b = c;
    } else if (h < 5.0F) {
        r = x, b = c;
    } else {
        r = c, b = x;
    }
    return ColorRgba{r + m, g + m, b + m, alpha};
}

}  // namespace

ColorRgba adjustColor(const ColorRgba& color, double hueShift, double saturation, double brightness) {
    if (hueShift == 0.0 && saturation == 1.0 && brightness == 1.0) {
        return color;  // Standardwerte: die Farbe bleibt bitgenau
    }
    Hsv hsv = toHsv(color);
    hsv.h += static_cast<float>(hueShift);
    hsv.s = std::clamp(hsv.s * static_cast<float>(saturation), 0.0F, 1.0F);
    hsv.v = std::clamp(hsv.v * static_cast<float>(brightness), 0.0F, 1.0F);
    return fromHsv(hsv, color.a);
}

namespace {

// Überblendet zwischen 2 bis 4 Farben. `position` 0..1 läuft von der ersten zur letzten Farbe; mit
// `loop` geht es von der letzten wieder zur ersten (eine Periode von `position` ist ein voller Umlauf,
// ein Sägezahn-LFO zyklisiert also ewig durch alle Farben). Ohne LFO läuft der Node von selbst: `speed` sind
// Umläufe pro Sekunde (die Phase wird aufsummiert und zu `position` addiert; negativ rückwärts, 0 = steht). Ohne
// `loop` pendelt er dabei zwischen erster und letzter Farbe hin und her.
class ColorRampKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const std::array<ColorRgba, 4> colors{in.color("color1", ColorRgba{0.0F, 0.2F, 1.0F, 1.0F}),
                                              in.color("color2", ColorRgba{1.0F, 0.3F, 0.0F, 1.0F}),
                                              in.color("color3", ColorRgba{0.0F, 1.0F, 0.3F, 1.0F}),
                                              in.color("color4", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})};
        const auto count = static_cast<std::size_t>(std::clamp<std::int64_t>(in.integer("count", 2), 2, 4));
        const bool loop = in.boolean("loop", true);
        const double speed = in.number("speed", 0.0);
        if (std::isfinite(speed)) {
            phase_ += speed * context.time.delta.count();
            phase_ -= std::floor(phase_);  // nur der Bruchteil zählt; so bleibt die Zahl klein und genau
        }
        const double position = in.number("position", 0.0) + phase_;

        const double segments = static_cast<double>(loop ? count : count - 1);
        double p = 0.0;
        if (loop) {
            p = position - std::floor(position);
        } else if (speed != 0.0) {
            const double t = position - 2.0 * std::floor(position / 2.0);  // Dreieck: 0 -> 1 -> 0
            p = t <= 1.0 ? t : 2.0 - t;
        } else {
            p = std::clamp(position, 0.0, 1.0);
        }
        const double t = p * segments;
        const auto index = std::min(static_cast<std::size_t>(t), static_cast<std::size_t>(segments) - 1);
        const auto f = static_cast<float>(t - static_cast<double>(index));
        const ColorRgba& a = colors[index];
        const ColorRgba& b = colors[(index + 1) % count];
        outputs.set("out", ColorRgba{a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f,
                                     a.a + (b.a - a.a) * f});
    }

private:
    double phase_{0.0};  // aufsummierter Umlauf durch `speed`, 0..1
};

}  // namespace

values::CpuNodeDefinition colorRampNode() {
    return cpuNode("wh.value.colorramp", "Color Ramp", "Value", {valueOutput("out", "Out", graph::PortType::Color)},
                   {integer("count", "Colors", 2, 2.0, 4.0), number("position", "Position", 0.0),
                    number("speed", "Speed (cycles/s)", 0.2, -10.0, 10.0),
                    toggle("loop", "Loop back to first color", true),
                    color("color1", "Color 1", ColorRgba{0.0F, 0.2F, 1.0F, 1.0F}),
                    color("color2", "Color 2", ColorRgba{1.0F, 0.3F, 0.0F, 1.0F}),
                    color("color3", "Color 3", ColorRgba{0.0F, 1.0F, 0.3F, 1.0F}),
                    color("color4", "Color 4", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
                   kernelFactory<ColorRampKernel>());
}

}  // namespace whitehole::nodes::detail
