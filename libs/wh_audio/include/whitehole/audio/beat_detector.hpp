// beat_detector.hpp: Beat- und Tempo-Erkennung aus dem Spektrum.
//
// Warum es diese Datei gibt: Aus dem Spektrum je Analyseschritt ("Hop") entscheidet sie, ob gerade
// ein Schlag fällt, und schätzt das Tempo. Sie kennt weder Audiogerät noch FFT; sie bekommt nur eine
// Zahlenreihe und lässt sich deshalb mit künstlichen Signalen genau prüfen.
//
// Verfahren (Standard, bewusst einfach): Spektralfluss der unteren Bänder (nur Anstiege, in dB-
// normierter Skala), adaptive Schwelle aus Mittelwert und Streuung der letzten Sekunde, lokales
// Maximum über der Schwelle, Mindestabstand zwischen zwei Schlägen. Das Tempo ist der Median der
// letzten Schlagabstände, in den Bereich 70..160 BPM gefaltet (Verdopplung/Halbierung).
// Grenzen: Das ist eine Heuristik für Musik mit erkennbarer Bassbetonung; bei dichtem Material ohne
// Kick oder bei Tempowechseln irrt sie sich. Dafür gibt es den Tap-Tempo-Node.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

#include "whitehole/core/spectrum.hpp"

namespace whitehole::audio {

struct BeatInfo {
    std::uint64_t count{0};   // Zahl der bisher erkannten Schläge
    float strength{0.0F};     // Stärke des letzten Schlags: Fluss / Schwelle (>= 1)
    float bpm{0.0F};          // geschätztes Tempo, 0 solange es zu wenige Schläge gibt
    double lastBeatTime{0.0}; // Audiozeit (Sekunden seit Start) des letzten Schlags
};

class BeatDetector {
public:
    /// `hopSeconds`: Zeit zwischen zwei `push`-Aufrufen. `minInterval`: kürzester Abstand zweier
    /// Schläge in Sekunden (0,3 entspricht höchstens 200 BPM).
    explicit BeatDetector(double hopSeconds, double minInterval = 0.3);

    /// Ein Analyseschritt. `time` ist die Audiozeit dieses Schritts in Sekunden.
    void push(const Spectrum& spectrum, double time);

    [[nodiscard]] const BeatInfo& info() const noexcept { return info_; }
    void reset();

private:
    void updateTempo();

    double hopSeconds_;
    double minInterval_;
    Spectrum previous_{};
    bool hasPrevious_{false};
    std::array<double, 3> recent_{};      // Fluss der letzten drei Schritte: [0] ältester
    std::array<double, 3> recentTime_{};
    std::deque<double> history_;          // Fluss der letzten ~Sekunde (Schwelle)
    std::deque<double> beatTimes_;        // letzte Schlagzeiten
    BeatInfo info_;
};

}  // namespace whitehole::audio
