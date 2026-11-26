// phase_tracker.hpp: merkt sich je Reglerwert einen Versatz, damit eine Animation beim Verstellen des Reglers
// nicht springt.
//
// Warum es diese Datei gibt: Viele Nodes rechneten `Zeit * Regler` (z. B. `@time * @param(speed)`). Wird der Regler
// verstellt, ändert sich das ganze Produkt auf einmal: Beim Labyrinth tauchten die Läufer an neuen Stellen auf, bei
// Fire, Ripple, Tunnel und anderen sprang das Bild (Rückmeldung vom 26.11.). Richtig ist die Phase als Integral der
// Rate über die Zeit. Statt eine Summe über alle Frames zu führen (die beim Springen der Zeit falsch würde), gilt hier
//     phase(t) = versatz + t * rate
// mit einem Versatz, der nur dann angepasst wird, wenn sich die Rate ändert - und zwar so, dass die Phase im
// Augenblick der Änderung gleich bleibt. Solange der Regler unberührt ist, ist der Versatz exakt 0 und die Phase
// bitgleich zu `t * rate` wie bisher. Die Zeit darf beliebig springen (Zeitleiste, Neustart): Die Phase ist immer
// eine reine Funktion der Zeit für den aktuellen Reglerstand.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/phase_offset.hpp"

namespace whitehole::compiler {

struct ParameterLayout;

class PhaseTracker {
public:
    /// Phase des Phase-Slots `key` (Schlüssel des Slots, siehe phaseSlotKey) an Node `node` zur Zeit `time` (Sekunden) bei der Rate `rate`. Der erste Aufruf legt
    /// den Eintrag an (Versatz 0), jede Änderung der Rate danach verschiebt den Versatz so, dass die Phase bei
    /// `time` unverändert bleibt. Eine nicht endliche Rate ändert nichts und liefert `time * rate`.
    [[nodiscard]] double phase(NodeId node, std::string_view key, double time, double rate);

    /// Vergisst alle Einträge, die das Layout nicht (mehr) als Phase-Slot führt (Node entfernt, Platzhalter weg).
    void retain(const ParameterLayout& layout);

    void clear() noexcept { entries_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
    std::map<std::pair<NodeId::rep_type, std::string>, PhaseOffset> entries_;
};

}  // namespace whitehole::compiler
