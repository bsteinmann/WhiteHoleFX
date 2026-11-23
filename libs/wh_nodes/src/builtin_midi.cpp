// MIDI-Eingang als Nodes: "MIDI CC" liest einen Regler (Control Change) als Zahl, "MIDI Note" eine Taste als
// Schalter plus Anschlagstärke.
//
// Warum es diese Datei gibt: Wie bei DMX und Audio liegt der Empfang (RtMidi, eigener Thread) in einer eigenen
// Bibliothek (wh_midi) und weiß nichts von Nodes. Hier holen sich die Kernel pro Frame den Stand aus dem MidiStore
// (kurzer Mutex, kein Warten auf das Gerät) und sind damit für den Engine-Thread geeignet.
//
// Unterschied zu DMX: Ein Regler meldet sich nur bei Bewegung. Darum gilt "Signal" ab der ersten Nachricht und bleibt
// es (Standard: kein Zeitlimit); der letzte Wert wird gehalten. Wie jede Hardware-Quelle ist der Wert nicht aus der
// Projektzeit ableitbar; Offline-Rendern mit MIDI-Nodes ist daher nicht reproduzierbar.
#include <algorithm>
#include <cstdint>
#include <memory>

#include "builtin_internal.hpp"
#include "whitehole/midi/midi_store.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

std::uint8_t channelOf(const values::KernelInputs& in) {
    return static_cast<std::uint8_t>(std::clamp<std::int64_t>(in.integer("channel", 1), 1, 16) - 1);
}

std::uint8_t numberOf(const values::KernelInputs& in, const char* key) {
    return static_cast<std::uint8_t>(std::clamp<std::int64_t>(in.integer(key, 0), 0, 127));
}

bool fresh(double ageSeconds, double timeout) { return timeout <= 0.0 || ageSeconds <= timeout; }

class MidiCcKernel final : public values::ICpuKernel {
public:
    explicit MidiCcKernel(std::shared_ptr<midi::MidiStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const auto sample = store_->readCc(channelOf(in), numberOf(in, "cc"));
        const bool present = sample && fresh(sample->ageSeconds, in.number("timeout", 0.0));
        if (present) {
            normalized_ = sample->value / 127.0;
        } else if (!in.boolean("hold", true)) {
            normalized_ = 0.0;
        }
        const double low = in.number("min", 0.0);
        const double high = in.number("max", 1.0);
        outputs.set("value", low + (high - low) * normalized_);
        outputs.set("on", normalized_ >= in.number("threshold", 0.5));
        outputs.set("present", present);
    }

private:
    std::shared_ptr<midi::MidiStore> store_;
    double normalized_{0.0};
};

class MidiNoteKernel final : public values::ICpuKernel {
public:
    explicit MidiNoteKernel(std::shared_ptr<midi::MidiStore> store) : store_{std::move(store)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        const auto sample = store_->readNote(channelOf(in), numberOf(in, "note"));
        const bool present = sample && fresh(sample->ageSeconds, in.number("timeout", 0.0));
        const bool on = present && sample->on;
        if (on) {
            velocity_ = sample->velocity / 127.0;
        } else if (!present && !in.boolean("hold", true)) {
            velocity_ = 0.0;
        }
        outputs.set("on", on);
        outputs.set("velocity", velocity_);
        outputs.set("present", present);
    }

private:
    std::shared_ptr<midi::MidiStore> store_;
    double velocity_{0.0};  // Velocity des letzten Anschlags (bleibt nach dem Loslassen stehen)
};

std::vector<graph::ParameterDescriptor> linkParameters(const char* numberKey, const char* numberName) {
    return {integer("channel", "Channel", 1, 1.0, 16.0),
            integer(numberKey, numberName, 0, 0.0, 127.0),
            number("timeout", "Signal timeout (s, 0 = never)", 0.0, 0.0, 600.0),
            toggle("hold", "Hold last value on signal loss", true)};
}

}  // namespace

std::vector<values::CpuNodeDefinition> midiNodes(std::shared_ptr<midi::MidiStore> store) {
    using graph::PortType;
    if (!store) {
        store = std::make_shared<midi::MidiStore>();  // ohne Gerät: dauerhaft "kein Signal"
    }
    std::vector<values::CpuNodeDefinition> nodes;

    auto ccParameters = linkParameters("cc", "Controller (CC number)");
    ccParameters.push_back(number("min", "Out min", 0.0));
    ccParameters.push_back(number("max", "Out max", 1.0));
    ccParameters.push_back(number("threshold", "On threshold", 0.5, 0.0, 1.0));
    nodes.push_back(cpuNode("wh.value.midicc", "MIDI CC", "Input",
                            {valueOutput("value", "Value", PortType::Scalar), valueOutput("on", "On", PortType::Boolean),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            std::move(ccParameters), [store] { return std::make_unique<MidiCcKernel>(store); }));

    nodes.push_back(cpuNode("wh.value.midinote", "MIDI Note", "Input",
                            {valueOutput("on", "On", PortType::Boolean), valueOutput("velocity", "Velocity", PortType::Scalar),
                             valueOutput("present", "Signal", PortType::Boolean)},
                            linkParameters("note", "Note number"),
                            [store] { return std::make_unique<MidiNoteKernel>(store); }));
    return nodes;
}

}  // namespace whitehole::nodes::detail
