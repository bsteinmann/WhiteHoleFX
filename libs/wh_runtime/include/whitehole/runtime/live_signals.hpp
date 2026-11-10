// live_signals.hpp: die Werte der CPU-Nodes im letzten Frame, aufbereitet für die Anzeige im Node-Editor.
//
// Warum es diese Datei gibt: Der Editor soll in den Nodes Pegelbalken und an den Kabeln Helligkeit zeigen, damit man
// Regler und Audio-Eingänge nach Augenmaß einstellen kann. Der ValueStore (wh_values) liefert dafür rohe
// Variants; hier werden sie zu drei einfachen Arten (Zahl, Balkenreihe, Farbe) verdichtet, ohne Qt und ohne GPU,
// damit das Verhalten testbar ist. LiveRange liefert zu jeder Zahl die Balkenlänge 0..1.
// Abhängigkeiten: wh_values, wh_core.
#pragma once

#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/spectrum.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/values/value_store.hpp"

namespace whitehole::runtime {

enum class LiveSignalKind {
    Number,  ///< Scalar, Integer, Boolean, Trigger: `number`
    Bars,    ///< Spektrum oder Wellenform: `bars` (Spektrum 0..1, Welle -1..1)
    Color,   ///< Farbe: `color`
};

struct LiveSignal {
    NodeId node;
    std::string portKey;
    LiveSignalKind kind{LiveSignalKind::Number};
    double number{0.0};
    Spectrum bars{};
    ColorRgba color{};
};

/// Alle Werte des Stores als Liste. Texte und Vektoren werden übersprungen (sie haben keinen sinnvollen Pegel).
/// Die Reihenfolge ist nicht festgelegt.
[[nodiscard]] std::vector<LiveSignal> collectLiveSignals(const values::ValueStore& store);

/// Merkt sich den Wertebereich eines Ausgangs, damit der Balken auch bei Werten außerhalb 0..1 lesbar bleibt.
/// Werte in 0..1 zeigen direkt ihren Wert; größere oder negative Werte weiten den Bereich auf, der danach
/// langsam wieder auf 0..1 zurückfällt.
class LiveRange {
public:
    /// Nimmt einen neuen Wert auf und liefert die Balkenlänge 0..1.
    float update(double value) noexcept;
    [[nodiscard]] double low() const noexcept { return low_; }
    [[nodiscard]] double high() const noexcept { return high_; }

private:
    double low_{0.0};
    double high_{1.0};
};

}  // namespace whitehole::runtime
