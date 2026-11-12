// DMX-Ausgabe: Werte aus dem Node-Graph auf DMX-Kanäle schreiben (Art-Net oder sACN), unabhängig von LEDs.
//
// Warum es diese Datei gibt: Nicht alles an einer Bühne ist ein Pixel. Nebel, Dimmer, ein Moving Head oder ein
// Relais hängen an DMX-Kanälen, und ihre Werte sollen aus demselben Graph kommen wie die Effekte (LFO auf den Dimmer,
// Audio-Trigger auf den Strobe, Fader-Regler auf den Nebel). Die beiden Nodes hier sind Senken: keine Ausgänge, darum
// wertet der Evaluator sie immer aus (value_evaluator.hpp). Je Frame legen sie einen ChannelWrite in den ValueStore;
// die Session reicht ihn an die Ausgabe-Pipeline weiter (output_pipeline_channels.cpp), die ihn in die Universen
// einträgt und mit der Frame-Rate sendet, solange "Ausgabe senden" an ist.
//
//   wh.output.dmx      "DMX Out":   acht Eingänge 0..1 auf acht Kanäle ab `channel` (16 Bit: je zwei Kanäle, grob/fein),
//                                    `low`/`high` legen fest, welche DMX-Werte 0 und 1 entsprechen (zum Beispiel 10..200).
//   wh.output.dmxcolor "DMX Color": eine Farbe auf RGB oder RGBW, davor auf Wunsch ein Dimmer-Kanal.
// `destination` ist die Controller-Nummer wie im Patch (Reiter Ausgabe); das Protokoll bestimmt der Controller.
#include <algorithm>
#include <cmath>

#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

constexpr int kChannelInputs = 8;

std::uint8_t dmxByte(double value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
}

/// Kopf eines ChannelWrite aus den gemeinsamen Parametern.
ChannelWrite header(const values::KernelInputs& in) {
    ChannelWrite write;
    write.destination = static_cast<std::uint32_t>(std::clamp<std::int64_t>(in.integer("destination", 0), 0, 255));
    write.universe = static_cast<std::uint32_t>(std::clamp<std::int64_t>(in.integer("universe", 0), 0, 63999));
    write.firstChannel = static_cast<std::uint16_t>(std::clamp<std::int64_t>(in.integer("channel", 1), 1, 512));
    return write;
}

class DmxOutKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        ChannelWrite write = header(in);
        const bool wide = in.boolean("bits16", false);
        const double low = std::clamp(in.number("low", 0.0), 0.0, 255.0);
        const double high = std::clamp(in.number("high", 255.0), 0.0, 255.0);
        const std::int64_t count = std::clamp<std::int64_t>(in.integer("count", kChannelInputs), 1, kChannelInputs);
        for (std::int64_t i = 1; i <= count; ++i) {
            const double v = std::clamp(in.number("ch" + std::to_string(i), 0.0), 0.0, 1.0);
            const double scaled = low + (high - low) * v;  // in DMX-Werten 0..255
            if (wide) {
                // 16 Bit: 0..255 auf 0..65535 strecken (255 -> 65535), grob zuerst
                const auto word = static_cast<std::uint32_t>(std::lround(std::clamp(scaled, 0.0, 255.0) * 257.0));
                write.data.push_back(static_cast<std::uint8_t>(word >> 8));
                write.data.push_back(static_cast<std::uint8_t>(word & 0xFFU));
            } else {
                write.data.push_back(dmxByte(scaled));
            }
        }
        outputs.setChannels(std::move(write));
    }
};

class DmxColorOutKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        ChannelWrite write = header(in);
        const ColorRgba color = in.color("color", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
        const std::int64_t mode = in.integer("mode", 0);  // 0 RGB, 1 RGBW, 2 Dimmer+RGB, 3 Dimmer+RGBW
        const double dimmer = std::clamp(in.number("dimmer", 1.0), 0.0, 1.0);
        const double alpha = std::clamp(static_cast<double>(color.a), 0.0, 1.0);
        const auto channel = [&](float c) { return dmxByte(std::clamp(static_cast<double>(c), 0.0, 1.0) * alpha * 255.0); };
        if (mode >= 2) {
            write.data.push_back(dmxByte(dimmer * 255.0));
        }
        write.data.push_back(channel(color.r));
        write.data.push_back(channel(color.g));
        write.data.push_back(channel(color.b));
        if (mode == 1 || mode == 3) {
            // Weiß: der gemeinsame Anteil der drei Farben (so leuchtet Weiß auch wirklich über den Weiß-Kanal)
            write.data.push_back(channel(std::min({color.r, color.g, color.b})));
        }
        outputs.setChannels(std::move(write));
    }
};

std::vector<graph::ParameterDescriptor> targetParameters() {
    return {integer("destination", "Controller (number from Output page)", 0, 0.0, 255.0),
            integer("universe", "Universe (Art-Net from 0, sACN from 1)", 0, 0.0, 63999.0),
            integer("channel", "First channel", 1, 1.0, 512.0)};
}

}  // namespace

std::vector<values::CpuNodeDefinition> dmxOutNodes() {
    std::vector<values::CpuNodeDefinition> nodes;

    auto outParameters = targetParameters();
    outParameters.push_back(integer("count", "Channels used (ch1 ...)", kChannelInputs, 1.0, kChannelInputs));
    outParameters.push_back(toggle("bits16", "16 bit (two channels each, coarse + fine)", false));
    outParameters.push_back(number("low", "DMX value at 0", 0.0, 0.0, 255.0));
    outParameters.push_back(number("high", "DMX value at 1", 255.0, 0.0, 255.0));
    for (int i = 1; i <= kChannelInputs; ++i) {
        outParameters.push_back(number("ch" + std::to_string(i), "Channel " + std::to_string(i), 0.0, 0.0, 1.0));
    }
    nodes.push_back(cpuNode("wh.output.dmx", "DMX Out", "Output", {}, std::move(outParameters), kernelFactory<DmxOutKernel>()));

    auto colorParameters = targetParameters();
    colorParameters.push_back(choice("mode", "Channels", {"RGB (3)", "RGBW (4)", "Dimmer + RGB (4)", "Dimmer + RGBW (5)"}));
    colorParameters.push_back(color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}));
    colorParameters.push_back(number("dimmer", "Dimmer (with dimmer channel)", 1.0, 0.0, 1.0));
    nodes.push_back(cpuNode("wh.output.dmxcolor", "DMX Color Out", "Output", {}, std::move(colorParameters),
                            kernelFactory<DmxColorOutKernel>()));
    return nodes;
}

}  // namespace whitehole::nodes::detail
