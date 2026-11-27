// tempo_clock.hpp: eine gemeinsame Tempo-Uhr (BPM, Beat, Phase) für alle Looks.
//
// Warum es diese Datei gibt: Tempo-Nodes wie Clock oder Tap Tempo halten ihren Zustand im eigenen Rechenkern; der geht
// verloren, sobald der Look wechselt (die Live-Decks entfernen dessen Nodes). Die Tempo-Uhr lebt stattdessen im Programm
// (wie AudioStore oder MidiStore, siehe BuiltinLibrary) und überlebt Look- und Projektwechsel: alle Looks teilen dasselbe
// Tempo und denselben Beat. Wunsch vom 27.11., Backlog in CLAUDE.md ("Globale Tempo-Uhr").
//
// Zeitbezug: Die Uhr hat keine eigene Uhrzeit. Der Player schiebt sie einmal pro Bild mit update(now) weiter (seine Live-Uhr,
// in Tests vorgegeben), die Rechenkerne lesen danach nur snapshot(). Dadurch sehen alle Nodes eines Bildes denselben Stand.
//
// Quellen: Tap (tap(), z. B. Taste im Live-Fenster, MIDI-Taste, DMX-Kanal), ein fester Wert, Audio-Beat (externalBeat(): jeder
// erkannte Schlag ist ein Beat, das Tempo kommt vom Erkenner) und ein DMX-Kanal als BPM-Wert (externalBpm(), nur Tempo, keine
// Phase). Die Uhr kennt weder Audio noch DMX: der Player liest beide und meldet sie hier. MIDI-Clock folgt in Stufe 3.
//
// Phase: zählt Beats (0, 1, 2, ...). Ändert sich das Tempo, läuft sie stetig weiter. Ein Tap setzt den Beat auf den Zeitpunkt
// des Taps (die Phase "rastet" auf die nächste ganze Zahl ein); der Sprung wird nicht hart gezeigt, sondern in
// kSettleSeconds auf dem kürzesten Weg angeglichen, damit Effekte nicht zucken.
//
// Thread-sicher: tap() und die Einstellungen dürfen aus der Oberfläche kommen, update() und snapshot() aus dem Engine-Takt.
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace whitehole::values {

class TempoClock {
public:
    enum class Source : std::uint8_t { Tap, Fixed, Audio, Dmx };

    /// Dauer, über die ein Einrasten der Phase angeglichen wird.
    static constexpr double kSettleSeconds = 0.25;
    static constexpr double kDefaultBpm = 120.0;
    static constexpr double kMinBpm = 30.0;
    static constexpr double kMaxBpm = 300.0;
    /// Längere Pause zwischen zwei Taps beginnt eine neue Folge (das Tempo bleibt, nur die Phase rastet ein).
    static constexpr double kTapTimeout = 2.0;

    /// Stand eines Bildes.
    struct Snapshot {
        double bpm{kDefaultBpm};
        double beats{0.0};        // durchgehende Phase in Beats (angeglichen)
        double phase{0.0};        // Bruchteil des laufenden Beats, 0..1
        std::int64_t beat{0};     // Nummer des laufenden Beats
        bool beatNow{false};      // in diesem Bild beginnt ein Beat (nie zweimal derselbe)
        double sinceBeat{0.0};    // Sekunden seit dem letzten Beat
        Source source{Source::Tap};
    };

    void setSource(Source source);
    /// Fester Wert (wird auf kMinBpm..kMaxBpm begrenzt). Wirkt sofort, wenn die Quelle Fixed ist.
    void setFixedBpm(double bpm);
    [[nodiscard]] Source source() const;
    [[nodiscard]] double fixedBpm() const;

    /// Ein Tap; wird mit dem nächsten update() zu dessen Zeit verrechnet. Nur bei Quelle Tap wirksam.
    void tap();

    /// Ein Beat von außen (Audio-Erkenner), mit dessen geschätztem Tempo (0 = Tempo unverändert). Wird wie ein Tap mit dem
    /// nächsten update() verrechnet (die Phase rastet ein), das Tempo kommt aber vom Erkenner. Nur bei Quelle Audio wirksam.
    void externalBeat(double bpm);
    /// Ein Tempo von außen (DMX-Kanal); die Phase läuft stetig weiter. Nur bei Quelle Dmx wirksam; 0 oder ungültig = unverändert.
    void externalBpm(double bpm);

    /// Schiebt die Uhr auf die Zeit `now` (Sekunden, monoton) und verrechnet die seit dem letzten Aufruf eingegangenen Taps.
    /// Ein Rückwärtsschritt zählt als Stillstand.
    void update(double now);
    [[nodiscard]] Snapshot snapshot() const;

private:
    void reanchor(double now, double bpm);  // ändert das Tempo, die Phase läuft stetig weiter
    void snapTo(double now, double targetBeats);  // setzt die Phase, angeglichen über kSettleSeconds
    void handleTap(double now);
    void pullBeat(double now);  // rastet die Phase auf die nächste ganze Zahl ein und meldet den Beat (Tap, Audio)
    [[nodiscard]] double targetBeats(double now) const;
    [[nodiscard]] double shownBeats(double now) const;

    mutable std::mutex mutex_;
    Source source_{Source::Tap};
    double fixedBpm_{kDefaultBpm};
    double bpm_{kDefaultBpm};
    double anchorTime_{0.0};
    double anchorBeats_{0.0};
    double settleOffset_{0.0};  // gezeigte minus Ziel-Phase beim Einrasten, klingt in kSettleSeconds ab
    double settleStart_{0.0};
    std::optional<double> now_;
    int pendingTaps_{0};
    int pendingBeats_{0};
    double pendingBeatBpm_{0.0};
    std::optional<double> lastTap_;
    std::vector<double> intervals_;
    std::int64_t lastBeat_{0};
    double lastBeatTime_{0.0};
    bool beatNow_{false};
};

}  // namespace whitehole::values
