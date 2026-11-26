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
    return entries_[std::make_pair(node.value(), std::string{key})].phase(time, rate);
}

void PhaseTracker::retain(const ParameterLayout& layout) {
    std::set<std::pair<NodeId::rep_type, std::string>> wanted;
    for (const ParameterSlot& slot : layout.slots) {
        if (slot.origin == SlotOrigin::Phase) {
            wanted.emplace(slot.node.value(), slot.key);
        }
    }
    for (auto it = entries_.begin(); it != entries_.end();) {
        it = wanted.contains(it->first) ? std::next(it) : entries_.erase(it);
    }
}

}  // namespace whitehole::compiler
