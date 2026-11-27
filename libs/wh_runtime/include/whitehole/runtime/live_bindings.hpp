// live_bindings.hpp: Belegung der Steuerfunktionen im Live-Modus (GO, Live beenden, Master, Überblendzeit, manuelle
// Überblendung, Tempo-Tap) auf MIDI und DMX.
//
// Warum es das gibt: Die festen Nummern eines Controller-Profils (midi/live_profile.hpp) sind nur der Anfang. Jede Funktion
// lässt sich per "Lernen" auf eine andere Taste, einen anderen Regler oder einen DMX-Kanal legen - im Reiter
// "Steuerung" der Einstellungen und im Live-Fenster, die beide genau diese Belegung schreiben (eine Wahrheit).
// Gespeichert wird sie in den Programmeinstellungen als Textzeilen (serializeBindings/parseBindings).
//
// MIDI: ohne eigene Belegung gilt das Profil ("Auf Profil zurücksetzen" löscht die eigene). DMX: ohne Belegung aus.
// Ausnahme Master über DMX: das ist die schon vorhandene Einstellung "Master über DMX" (Seite DMX-Eingang); der Reiter
// "Steuerung" zeigt und ändert genau sie, darum hat LiveBindings dafür kein eigenes Feld.
// Entwurf: docs/entwurf-midi.md, Abschnitt 6.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "whitehole/input/dmx_learn.hpp"
#include "whitehole/midi/live_profile.hpp"

namespace whitehole::runtime {

/// Tap: ein Schlag für die gemeinsame Tempo-Uhr (values::TempoClock); wirkt auch außerhalb des Live-Betriebs. Das Profil
/// sieht dafür keine Taste vor (nur eigene Belegung).
enum class LiveFunction : std::uint8_t { Go, Stop, Master, FadeTime, Crossfade, Tap };
inline constexpr std::size_t kLiveFunctionCount = 6;
inline constexpr std::array<LiveFunction, kLiveFunctionCount> kLiveFunctions{
    LiveFunction::Go, LiveFunction::Stop, LiveFunction::Master, LiveFunction::FadeTime, LiveFunction::Crossfade, LiveFunction::Tap};

/// Taste (Note, bei DMX: ab 128 = gedrückt) oder Regler (CC, bei DMX: der Wert).
[[nodiscard]] constexpr bool isButton(LiveFunction function) noexcept {
    return function == LiveFunction::Go || function == LiveFunction::Stop || function == LiveFunction::Tap;
}
/// Ob die Funktion eine eigene DMX-Belegung hat (Master nicht, siehe oben).
[[nodiscard]] constexpr bool hasDmxBinding(LiveFunction function) noexcept { return function != LiveFunction::Master; }

struct MidiBinding {
    int channel{0};   // 0..15
    int number{-1};   // Note bzw. CC; -1 = nicht belegt
    bool operator==(const MidiBinding&) const = default;
};

struct LiveBindings {
    std::array<std::optional<MidiBinding>, kLiveFunctionCount> midi{};  // leer = Profil
    std::array<std::optional<input::DmxAddress>, kLiveFunctionCount> dmx{};  // leer = aus
    bool operator==(const LiveBindings&) const = default;

    [[nodiscard]] std::optional<MidiBinding>& midiOf(LiveFunction f) { return midi[static_cast<std::size_t>(f)]; }
    [[nodiscard]] const std::optional<MidiBinding>& midiOf(LiveFunction f) const { return midi[static_cast<std::size_t>(f)]; }
    [[nodiscard]] std::optional<input::DmxAddress>& dmxOf(LiveFunction f) { return dmx[static_cast<std::size_t>(f)]; }
    [[nodiscard]] const std::optional<input::DmxAddress>& dmxOf(LiveFunction f) const { return dmx[static_cast<std::size_t>(f)]; }
};

/// Was das Profil für die Funktion vorsieht (Kanal 1).
[[nodiscard]] MidiBinding profileBinding(const midi::LiveProfile& profile, LiveFunction function) noexcept;
/// Eigene Belegung, sonst die des Profils.
[[nodiscard]] MidiBinding effectiveMidi(const LiveBindings& bindings, const midi::LiveProfile& profile, LiveFunction function) noexcept;

/// Textzeilen: "<funktion> midi <kanal> <nummer>" bzw. "<funktion> dmx <artnet|sacn> <universe> <kanal>";
/// Funktionen: go, stop, master, fade, crossfade, tap. Unlesbare Zeilen fallen beim Lesen weg.
[[nodiscard]] std::vector<std::string> serializeBindings(const LiveBindings& bindings);
[[nodiscard]] LiveBindings parseBindings(const std::vector<std::string>& lines);

}  // namespace whitehole::runtime
