// live_dmx.hpp: ein DMX-Kanal wählt den aktiven Look im Live-Betrieb, ein zweiter legt die Überblendzeit fest.
//
// Warum es diese Datei gibt (DMX-Fernsteuerung für Live-Modus, Teil 1): Am Lichtpult soll sich zwischen den
// gespeicherten Looks wechseln lassen, ohne Maus oder Tastatur. Welcher Kanal das ist, hängt vom Auftritt ab
// (Pult, Universe), nicht vom Projekt - genau wie beim Master (siehe master_dmx.hpp); die Oberfläche merkt es
// sich bei den Programmeinstellungen und reicht es dem Player.
//
// Kanalbelegung wie an einem "Szenen-Wahl"-Kanal gängiger Scheinwerfer: Am Auswahl-Kanal heißt 0 "kein Wechsel"
// (der zuletzt gezeigte Look bleibt), 1..255 teilt sich in so viele gleich große Stufen auf, wie das Projekt
// gerade Looks hat (die Zahl ändert sich mit dem Projekt, darum keine feste Tabelle wie bei einem Scheinwerfer
// mit fest eingebauten Programmen). Der zweite Kanal (optional, 0 = aus) überschreibt die im Look gespeicherte
// Überblendzeit: Kanalwert 0 = sofort, 255 = kLiveDmxMaxFadeSeconds, linear dazwischen.
#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>

#include "whitehole/input/dmx_store.hpp"

namespace whitehole::runtime {

struct LiveDmxSource {
    input::DmxProtocol protocol{input::DmxProtocol::ArtNet};
    std::uint32_t universe{0};
    std::uint32_t selectChannel{1};  // 1..512; der Wert 0 am Kanal selbst heißt "kein Wechsel"
    std::uint32_t fadeChannel{0};    // 0 = aus (der Look behält seine eigene Überblendzeit), sonst 1..512

    bool operator==(const LiveDmxSource&) const = default;
};

/// Bleibt das letzte Paket länger aus, hält die Auswahl ihren Stand (wie beim Master, siehe kMasterDmxTimeoutSeconds).
inline constexpr double kLiveDmxTimeoutSeconds = 5.0;
/// Obere Grenze der per Fade-Kanal einstellbaren Überblendzeit (Kanalwert 255 = diese Zeit, linear).
inline constexpr double kLiveDmxMaxFadeSeconds = 10.0;

/// Kanalwert (0..255) auf einen Look-Index (0..lookCount-1) abgebildet. 0 am Kanal oder keine Looks: kein
/// Wechsel (nullopt). Reine Rechnung, ohne Player - einzeln testbar.
[[nodiscard]] std::optional<int> lookIndexFromDmxValue(std::uint8_t value, std::size_t lookCount) noexcept;

}  // namespace whitehole::runtime
