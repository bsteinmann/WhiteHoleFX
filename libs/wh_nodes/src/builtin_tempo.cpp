// builtin_tempo.cpp: der Node "Tempo" (wh.value.tempo) liest die gemeinsame Tempo-Uhr des Programms.
//
// Warum es diese Datei gibt: Clock, Tap Tempo und Beat halten ihr Tempo im eigenen Rechenkern; nach einem Look-Wechsel
// fangen sie von vorn an. "Tempo" hat keinen eigenen Zustand, sondern liest values::TempoClock, die im Programm lebt und
// Look- und Projektwechsel überlebt: alle Looks teilen BPM, Beat und Phase. Quelle und fester Wert stellt man in den
// Einstellungen ein (Reiter Steuerung), getappt wird im Live-Fenster oder per MIDI/DMX. Wunsch vom 27.11. (CLAUDE.md).
// Nicht reproduzierbar beim Offline-Rendern: das Tempo kommt von außen, wie bei DMX- und MIDI-Nodes.
#include <algorithm>
#include <cmath>

#include "builtin_internal.hpp"
#include "whitehole/values/tempo_clock.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

class TempoKernel final : public values::ICpuKernel {
public:
    explicit TempoKernel(std::shared_ptr<values::TempoClock> clock) : clock_{std::move(clock)} {}

    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const values::TempoClock::Snapshot s = clock_->snapshot();
        const double decay = context.inputs.number("decay", 0.25);
        const double pulse = decay > 1e-6 ? std::exp(-s.sinceBeat / decay) : (s.beatNow ? 1.0 : 0.0);
        outputs.set("beat", s.beatNow);
        outputs.set("pulse", pulse);
        outputs.set("phase", s.phase);
        outputs.set("bpm", s.bpm);
        outputs.set("count", static_cast<std::int64_t>(s.beat));
    }

private:
    std::shared_ptr<values::TempoClock> clock_;
};

}  // namespace

std::vector<values::CpuNodeDefinition> tempoNodes(std::shared_ptr<values::TempoClock> clock) {
    if (!clock) {
        clock = std::make_shared<values::TempoClock>();  // ohne Programm-Uhr (Tests): eine eigene, die nie weiterläuft
    }
    using graph::PortType;
    std::vector<values::CpuNodeDefinition> nodes;
    nodes.push_back(cpuNode("wh.value.tempo", "Tempo", "Value",
                            {valueOutput("beat", "Beat", PortType::Trigger), valueOutput("pulse", "Pulse", PortType::Scalar),
                             valueOutput("phase", "Phase", PortType::Scalar), valueOutput("bpm", "BPM", PortType::Scalar),
                             valueOutput("count", "Count", PortType::Integer)},
                            {number("decay", "Pulse decay (s)", 0.25, 0.0, 5.0)},
                            [clock] { return std::make_unique<TempoKernel>(clock); }));
    return nodes;
}

}  // namespace whitehole::nodes::detail
