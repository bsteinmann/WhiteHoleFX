// phase_offset.hpp: Phase eines Reglers als `versatz + zeit * rate`, die beim Ändern der Rate nicht springt.
//
// Warum es diese Datei gibt: Zwei Stellen brauchen dieselbe Rechnung - der PhaseTracker des Feldcompilers (@phase im
// Shader) und die Werte-Kernel Time, LFO und Random (CPU). Ohne sie ändert jede Reglerbewegung das ganze Produkt
// `zeit * rate` auf einmal und die Animation springt (Rückmeldung vom 26.11.). Der Versatz ändert sich nur, wenn sich
// die Rate ändert, und zwar so, dass die Phase im selben Augenblick gleich bleibt; bei unberührter Rate ist er exakt 0
// und das Ergebnis bitgleich zu `zeit * rate`. Die Zeit darf springen (Zeitleiste, Neustart): Die Phase ist für den
// aktuellen Reglerstand immer eine reine Funktion der Zeit.
#pragma once

#include <cmath>

namespace whitehole {

class PhaseOffset {
public:
    /// Phase bei `time` (Sekunden) und Rate `rate`. Eine nicht endliche Zeit oder Rate ändert nichts und liefert
    /// `time * rate`. Der erste Aufruf legt die Rate fest (Versatz 0).
    [[nodiscard]] double phase(double time, double rate) noexcept {
        if (!std::isfinite(rate) || !std::isfinite(time)) {
            return time * rate;
        }
        if (!started_) {
            started_ = true;
            rate_ = rate;
            return time * rate;
        }
        if (rate != rate_) {
            offset_ += time * (rate_ - rate);  // stetig: versatz' + time * rate' == versatz + time * rate_alt
            rate_ = rate;
        }
        return offset_ + time * rate_;
    }

private:
    double offset_{0.0};
    double rate_{0.0};
    bool started_{false};
};

}  // namespace whitehole
