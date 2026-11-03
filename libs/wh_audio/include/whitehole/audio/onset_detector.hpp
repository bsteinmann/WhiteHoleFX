// onset_detector.hpp: erkennt Einsätze (Kick, Snare, Hi-Hat) in einem Frequenzbereich des Spektrums.
//
// Warum es diese Datei gibt: Der BeatDetector findet den Takt über die tiefen Bänder insgesamt und wartet
// auf das Maximum des Flusses. Für Licht im Takt der Musik braucht es aber getrennte Auslöser je Instrument,
// möglichst früh und auch bei leisen Schlägen. Dieser Erkenner schaut nur auf einen Bandbereich, löst schon
// beim Anstieg über die Schwelle aus (ohne das Maximum abzuwarten, das spart einen Analyseschritt) und
// rechnet relativ: Die Schwelle folgt dem Mittelwert der letzten Sekunde, ein leises Stück wird also
// genauso erkannt wie ein lautes. Er kennt weder Gerät noch FFT und ist mit künstlichen Signalen testbar.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include "whitehole/core/spectrum.hpp"

namespace whitehole::audio {

struct OnsetInfo {
    std::uint64_t count{0};    // Zahl der bisher erkannten Einsätze
    float strength{0.0F};      // Fluss / Schwelle beim letzten Einsatz (>= 1)
    double lastTime{0.0};      // Audiozeit des letzten Einsatzes in Sekunden
};

class OnsetDetector {
public:
    /// Bänder [firstBand, endBand) des Spektrums. `minInterval`: kürzester Abstand zweier Einsätze in Sekunden.
    OnsetDetector(double hopSeconds, std::size_t firstBand, std::size_t endBand, double minInterval);

    void push(const Spectrum& spectrum, double time);

    [[nodiscard]] const OnsetInfo& info() const noexcept { return info_; }
    void reset();

private:
    double hopSeconds_;
    std::size_t first_;
    std::size_t end_;
    double minInterval_;
    Spectrum previous_{};
    bool hasPrevious_{false};
    double previousFlux_{0.0};
    std::deque<double> history_;
    OnsetInfo info_;
};

}  // namespace whitehole::audio
