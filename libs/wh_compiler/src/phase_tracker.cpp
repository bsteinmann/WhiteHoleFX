// phase_tracker.cpp: Umsetzung zu phase_tracker.hpp. Warum der Versatz statt einer Summe, steht dort.
#include "whitehole/compiler/phase_tracker.hpp"

#include <cmath>
#include <iterator>
#include <set>

#include "whitehole/compiler/parameter_layout.hpp"

namespace whitehole::compiler {

double PhaseTracker::phase(NodeId node, std::string_view key, double time, double rate) {
    if (!std::isfinite(rate) || !std::isfinite(time)) {
        return time * rate;
    }
    const auto id = std::make_pair(node.value(), std::string{key});
    const auto found = entries_.find(id);
    if (found == entries_.end()) {
        entries_.emplace(id, Entry{0.0, rate});
        return time * rate;
    }
    Entry& entry = found->second;
    if (rate != entry.rate) {
        // Stetig bleiben: versatz' + time * rate' == versatz + time * rate_alt.
        entry.offset += time * (entry.rate - rate);
        entry.rate = rate;
    }
    return entry.offset + time * entry.rate;
}

void PhaseTracker::retain(const ParameterLayout& layout) {
    std::set<std::pair<NodeId::rep_type, std::string>> wanted;
    for (const ParameterSlot& slot : layout.slots) {
        if (slot.origin == SlotOrigin::Phase) {
            wanted.emplace(slot.node.value(), parameterKeyOfPhaseSlot(slot.key));
        }
    }
    for (auto it = entries_.begin(); it != entries_.end();) {
        it = wanted.contains(it->first) ? std::next(it) : entries_.erase(it);
    }
}

}  // namespace whitehole::compiler
