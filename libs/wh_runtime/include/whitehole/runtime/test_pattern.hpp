// test_pattern.hpp: Testbilder für die Inbetriebnahme.
//
// Warum es diese Datei gibt: Beim Aufbau einer Wand will man wissen, ob jede LED an ihrem Platz
// leuchtet, ob die Farbreihenfolge stimmt und ob die Kabel in der erwarteten Richtung laufen,
// und zwar unabhängig vom Effekt. Ein Testbild ersetzt darum die Farben des Effekts, bevor sie
// in die Ausgabe und in die Vorschau gehen. Es ist reine Rechnung über die LED-Karte (kein Gerät,
// kein Qt) und wird deshalb ohne GPU getestet. Ein Testbild gehört nicht zum Projekt: Es wird
// weder gespeichert noch rückgängig gemacht.
//
// Arten:
//   Color      alle LEDs in einer Farbe (Rot, Grün, Blau, Weiß ... zum Prüfen von Netzteil und Farbe)
//   Highlight  nur die gewählten Fixtures zu 100 % Weiß, der Rest aus (zum Finden eines Fixtures)
//   Sweep      ein Balken läuft entlang der Achse x, y oder z (prüft Lage und Ausrichtung)
//   ColorOrder Rot, Grün, Blau, Weiß nacheinander (stimmt die Farbreihenfolge des Patches?)
//   Chase      ein Lichtpunkt läuft je Fixture in Kabelreihenfolge (prüft Richtung und Lücken)
#pragma once

#include <cstdint>
#include <vector>

#include "whitehole/core/types.hpp"
#include "whitehole/mapping/led_map.hpp"

namespace whitehole::runtime {

enum class TestPatternKind : std::uint8_t { Off, Color, Highlight, Sweep, ColorOrder, Chase };
enum class SweepAxis : std::uint8_t { X, Y, Z };

struct TestPattern {
    TestPatternKind kind{TestPatternKind::Off};
    /// Nur Color.
    ColorRgba color{1.0F, 1.0F, 1.0F, 1.0F};
    /// Nur Sweep.
    SweepAxis axis{SweepAxis::X};
    /// Tempo in Takten pro Sekunde: Sweep = Durchläufe, ColorOrder = Farbwechsel, Chase = LEDs.
    /// Nicht endlich oder <= 0 gilt als Standardtempo der Art (siehe defaultTestRate).
    float rate{0.0F};
    /// Beschränkt das Bild auf diese Fixtures; alle anderen bleiben aus. Leer = alle Fixtures,
    /// außer bei Highlight (dort heißt leer: nichts leuchtet).
    std::vector<mapping::FixtureId> fixtures;

    [[nodiscard]] bool active() const noexcept { return kind != TestPatternKind::Off; }
};

/// Standardtempo der Art (Sweep 0,4 Durchläufe/s, ColorOrder 0,5 Wechsel/s, Chase 12 LEDs/s).
[[nodiscard]] float defaultTestRate(TestPatternKind kind) noexcept;

/// Anzeigename der Art, z. B. "Laufbalken".
[[nodiscard]] const char* testPatternName(TestPatternKind kind) noexcept;

/// Farben für alle LEDs der Karte zum Zeitpunkt `seconds` (seit Beginn des Testbilds). Die Werte
/// sind "wie gesehen" wie die des Effekts; Gamma wendet erst die Ausgabe an. Aus: alles schwarz.
[[nodiscard]] std::vector<ColorRgba> renderTestPattern(const TestPattern& pattern, const mapping::LedMap& map,
                                                       double seconds);

}  // namespace whitehole::runtime
