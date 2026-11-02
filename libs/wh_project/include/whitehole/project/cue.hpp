// cue.hpp: die Cuelist eines Projekts: Looks in fester Reihenfolge mit Auslösern und Zeiten.
//
// Warum es diese Datei gibt (Issue #21): Looks lassen sich einzeln abrufen (Live-Modus). Für eine Show braucht es
// eine Reihenfolge: Cue 1 blendet den ersten Look ein, Cue 2 den nächsten, jeder mit eigener Zeit und eigenem Auslöser.
// Das Modell hier ist nur Daten (Projektdatei, Oberfläche und Ablaufsteuerung teilen sich dasselbe); der Ablauf steht
// in wh_runtime (cue_runner.hpp).
//
// Wie am Lichtpult: Ein Cue wartet auf seinen Auslöser. Löst er aus, vergeht `delay`, dann blendet der Look in `fade`
// Sekunden über den vorigen (der alte blendet im selben Zug aus). Ein Cue ohne Look blendet nach Schwarz.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace whitehole::project {

enum class CueTrigger : std::uint8_t {
    Go,     // von Hand: Knopf GO oder Leertaste
    Time,   // selbsttätig `time` Sekunden, nachdem der vorige Cue fertig eingeblendet ist
    Key,    // eine bestimmte Taste, von jedem Stand aus
    Dmx,    // ein DMX-Kanal steigt auf `threshold` oder darüber
    Audio,  // der nächste Beat
};

enum class CueDmxProtocol : std::uint8_t { ArtNet, Sacn };

inline constexpr double kMaxCueSeconds = 3600.0;

struct Cue {
    std::string name;  // frei, darf leer sein
    std::string look;  // Name eines Looks der Liste; leer = Schwarz
    CueTrigger trigger{CueTrigger::Go};
    double delay{0.0};  // Sekunden zwischen Auslöser und Beginn der Überblendung
    double fade{2.0};   // Sekunden der Überblendung
    double time{0.0};   // Time: Wartezeit nach dem vorigen Cue
    std::string key;    // Key: Name der Taste ("F5", "A", "1")
    CueDmxProtocol dmxProtocol{CueDmxProtocol::ArtNet};
    std::uint32_t dmxUniverse{0};
    std::uint32_t dmxChannel{1};  // 1..512
    std::uint32_t dmxThreshold{128};  // 1..255

    bool operator==(const Cue&) const = default;
};

}  // namespace whitehole::project
