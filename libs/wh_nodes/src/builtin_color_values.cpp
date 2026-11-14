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

// Überblendet zwischen 2 bis 4 Farben. `position` 0..1 läuft von der ersten zur letzten Farbe; im Modus
// "Loop" geht es von der letzten wieder zur ersten (eine Periode von `position` ist ein voller Umlauf, ein
// Sägezahn-LFO zyklisiert also ewig durch alle Farben), im Modus "Pendulum" pendelt er stattdessen zwischen
// erster und letzter Farbe hin und her. Ohne LFO läuft der Node von selbst: `speed` sind Umläufe pro Sekunde
// (die Phase wird aufsummiert und zu `position` addiert; negativ rückwärts, 0 = steht, Standard). Mit `hard`
// springt der Node wie ein Sequencer-Schritt direkt auf die Farbe des aktuellen Segments, statt zur nächsten
// überzublenden.
//
// `advance` ist ein zweiter, unabhängiger Weg, den Node zu steuern: eine steigende Flanke schaltet wie ein
// Taster einen Schritt weiter, unabhängig von `position`/`speed`, immer hart (kein Überblenden). Sobald
// `advance` einmal benutzt wurde, übernimmt dieser Schrittzähler dauerhaft die Ausgabe (für Bühnensteuerung
// per Knopf statt nur Zeit/LFO); `mode` entscheidet dabei genauso über Loop/Pendulum am Ende.
class ColorRampKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const std::array<ColorRgba, 4> colors{in.color("color1", ColorRgba{0.0F, 0.2F, 1.0F, 1.0F}),
                                              in.color("color2", ColorRgba{1.0F, 0.3F, 0.0F, 1.0F}),
                                              in.color("color3", ColorRgba{0.0F, 1.0F, 0.3F, 1.0F}),
                                              in.color("color4", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})};
        const auto count = static_cast<std::size_t>(std::clamp<std::int64_t>(in.integer("count", 2), 2, 4));
        const bool loop = in.integer("mode", 1) == 1;  // 0 Pendulum, 1 Loop

        advanceEdge_.update(in.boolean("advance", false));
        if (advanceEdge_.rose()) {
            manualActive_ = true;
            ++manualCounter_;
        }
        if (manualActive_) {
            outputs.set("out", colors[manualIndex(manualCounter_, count, loop)]);
            return;
        }

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
        if (in.boolean("hard", false)) {
            // Harter Schritt: p gleichmäßig über alle `count` Farben verteilt (wie Sequencer-Schritte),
            // unabhängig von der Paar-Aufteilung, die das Überblenden unten braucht.
            const auto hardIndex = std::min(static_cast<std::size_t>(p * static_cast<double>(count)), count - 1);
            outputs.set("out", colors[hardIndex]);
            return;
        }
        const double t = p * segments;
        const auto index = std::min(static_cast<std::size_t>(t), static_cast<std::size_t>(segments) - 1);
        const ColorRgba& a = colors[index];
        const auto f = static_cast<float>(t - static_cast<double>(index));
        const ColorRgba& b = colors[(index + 1) % count];
        outputs.set("out", ColorRgba{a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f,
                                     a.a + (b.a - a.a) * f});
    }

private:
    // Loop: schlicht modulo. Pendulum: dieselbe periodische Spiegelung wie beim Sequencer (direction 2),
    // zustandslos aus dem reinen Zähler berechnet statt eine eigene Richtung mitzuführen.
    static std::size_t manualIndex(std::int64_t counter, std::size_t count, bool loop) {
        if (count <= 1) {
            return 0;
        }
        const auto n = static_cast<std::int64_t>(count);
        if (loop) {
            return static_cast<std::size_t>(((counter % n) + n) % n);
        }
        const std::int64_t period = std::max<std::int64_t>(2 * n - 2, 1);
        const std::int64_t p = ((counter % period) + period) % period;
        return static_cast<std::size_t>(p < n ? p : period - p);
    }

    double phase_{0.0};  // aufsummierter Umlauf durch `speed`, 0..1
    std::int64_t manualCounter_{0};
    bool manualActive_{false};
    EdgeDetector advanceEdge_;
};

}  // namespace

values::CpuNodeDefinition colorRampNode() {
    auto color3 = color("color3", "Color 3", ColorRgba{0.0F, 1.0F, 0.3F, 1.0F});
    color3.visibleWhenAtLeastKey = "count";
    color3.visibleWhenAtLeast = 3.0;
    auto color4 = color("color4", "Color 4", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F});
    color4.visibleWhenAtLeastKey = "count";
    color4.visibleWhenAtLeast = 4.0;
    return cpuNode("wh.value.colorramp", "Color Ramp", "Value",
                   {valueOutput("out", "Out", graph::PortType::Color)},
                   {integer("count", "Colors", 2, 2.0, 4.0), number("position", "Position", 0.0),
                    number("speed", "Speed (cycles/s)", 0.0, -10.0, 10.0),
                    choice("mode", "Mode", {"Pendulum", "Loop"}, 1),
                    toggle("hard", "Hard steps (no fade, like a sequencer)", false),
                    toggle("advance", "Advance (each switch on steps, like a button)", false),
                    color("color1", "Color 1", ColorRgba{0.0F, 0.2F, 1.0F, 1.0F}),
                    color("color2", "Color 2", ColorRgba{1.0F, 0.3F, 0.0F, 1.0F}), std::move(color3),
                    std::move(color4)},
                   kernelFactory<ColorRampKernel>());
}

}  // namespace whitehole::nodes::detail
