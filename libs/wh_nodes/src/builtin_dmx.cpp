// DMX-Eingang als Nodes: "DMX Channel" liest einen Kanal (oder zwei als 16 Bit) und liefert Zahl,
// Schalter und "Signal da"; "DMX Color" liest drei oder vier Kanäle als Farbe.
//
// Warum es diese Datei gibt: Die Netzwerkseite (Art-Net/sACN empfangen) liegt in wh_input und weiß
// nichts von Nodes. Hier hängen die Kernel am gemeinsamen DmxStore und holen sich pro Frame den
// aktuellen Wert. Die Kernel greifen nur lesend auf den Speicher zu (kurzer Mutex, kein Warten auf
// das Netzwerk) und sind damit für den Engine-Thread geeignet.
//
// Unterschied zu allen anderen Werte-Nodes: Sie sind nicht rein. Der Wert hängt vom Netzwerk ab,
// nicht von der Zeit allein; Offline-Rendern mit DMX-Nodes ist daher nicht reproduzierbar.
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "builtin_internal.hpp"
#include "whitehole/input/dmx_store.hpp"

namespace whitehole::nodes::detail {

namespace {

using input::DmxProtocol;
using input::DmxStore;
using values::KernelContext;
using values::KernelInputs;
using values::KernelOutputs;

struct Reading {
    bool present{false};
    std::array<std::uint8_t, 4> bytes{};
};

/// Liest `count` Kanäle gemäß Quelle/Universe/Kanal der Parameter. "present" heißt: Paket da und nicht
/// älter als das Zeitlimit (0 = kein Limit).
Reading readChannels(const DmxStore& store, const KernelInputs& in, std::uint32_t count) {
    const auto protocol = in.integer("source", 0) == 1 ? DmxProtocol::Sacn : DmxProtocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(std::clamp<std::int64_t>(in.integer("universe", 1), 0, 63999));
    const auto channel = static_cast<std::uint32_t>(std::clamp<std::int64_t>(in.integer("channel", 1), 1, 512));
    const double timeout = in.number("timeout", 2.0);

    Reading reading;
    const auto sample = store.read(protocol, universe, channel, count);
    if (sample && (timeout <= 0.0 || sample->ageSeconds <= timeout)) {
        reading.present = true;
        reading.bytes = sample->values;
    }
    return reading;
}

class DmxChannelKernel final : public values::ICpuKernel {
public:
    explicit DmxChannelKernel(std::shared_ptr<DmxStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const bool wide = in.boolean("bits16", false);
        const Reading reading = readChannels(*store_, in, wide ? 2U : 1U);

        const double low = in.number("min", 0.0);
        const double high = in.number("max", 1.0);
        if (reading.present) {
            normalized_ = wide ? (reading.bytes[0] * 256.0 + reading.bytes[1]) / 65535.0 : reading.bytes[0] / 255.0;
        } else if (!in.boolean("hold", true)) {
            normalized_ = 0.0;
        }
        outputs.set("value", low + (high - low) * normalized_);
        outputs.set("on", normalized_ >= in.number("threshold", 0.5));
        outputs.set("present", reading.present);
    }

private:
    std::shared_ptr<DmxStore> store_;
    double normalized_{0.0};  // letzter gültiger Wert (für "Halten" bei Signalverlust)
};

class DmxColorKernel final : public values::ICpuKernel {
public:
    explicit DmxColorKernel(std::shared_ptr<DmxStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const bool dimmer = in.integer("mode", 0) == 1;
        const Reading reading = readChannels(*store_, in, dimmer ? 4U : 3U);

        if (reading.present) {
            const double scale = dimmer ? reading.bytes[3] / 255.0 : 1.0;
            last_ = ColorRgba{static_cast<float>(reading.bytes[0] / 255.0 * scale),
                              static_cast<float>(reading.bytes[1] / 255.0 * scale),
                              static_cast<float>(reading.bytes[2] / 255.0 * scale), 1.0F};
        } else if (!in.boolean("hold", true)) {
            last_ = ColorRgba{0.0F, 0.0F, 0.0F, 1.0F};
        }
        outputs.set("out", last_);
    }

private:
    std::shared_ptr<DmxStore> store_;
    ColorRgba last_{0.0F, 0.0F, 0.0F, 1.0F};
};

const std::vector<std::string> kSources{"Art-Net", "sACN"};

std::vector<graph::ParameterDescriptor> linkParameters(std::int64_t channelCount) {
    return {choice("source", "Source", kSources),
            integer("universe", "Universe", 1, 0.0, 63999.0),
            integer("channel", "Channel", 1, 1.0, static_cast<double>(512 - channelCount + 1)),
            number("timeout", "Signal timeout (s, 0 = never)", 2.0, 0.0, 60.0),
            toggle("hold", "Hold last value on signal loss", true)};
}

}  // namespace

std::vector<values::CpuNodeDefinition> dmxNodes(std::shared_ptr<input::DmxStore> store) {
    using graph::PortType;
    if (!store) {
        store = std::make_shared<DmxStore>();  // ohne Empfänger: liefert dauerhaft "kein Signal"
    }
    std::vector<values::CpuNodeDefinition> nodes;

    auto channelParameters = linkParameters(2);
    channelParameters.push_back(toggle("bits16", "16 bit (channel + next)", false));
    channelParameters.push_back(number("min", "Out min", 0.0));
    channelParameters.push_back(number("max", "Out max", 1.0));
    channelParameters.push_back(number("threshold", "On threshold", 0.5, 0.0, 1.0));
    nodes.push_back(cpuNode("wh.value.dmx", "DMX Channel", "Input",
                            {valueOutput("value", "Value", PortType::Scalar), valueOutput("on", "On", PortType::Boolean),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            std::move(channelParameters), [store] { return std::make_unique<DmxChannelKernel>(store); }));

    auto colorParameters = linkParameters(4);
    colorParameters.push_back(choice("mode", "Channels", {"RGB (3)", "RGB + Dimmer (4)"}));
    nodes.push_back(cpuNode("wh.value.dmxcolor", "DMX Color", "Input", {valueOutput("out", "Out", PortType::Color)},
                            std::move(colorParameters), [store] { return std::make_unique<DmxColorKernel>(store); }));
    return nodes;
}

}  // namespace whitehole::nodes::detail
