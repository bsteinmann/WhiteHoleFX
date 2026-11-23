// live_profile.hpp: Belegung eines MIDI-Controllers für den Live-Modus (zuerst Akai APC mini mk1).
//
// Warum es das gibt: Welche Pad-Note welchen Look abruft, welche Taste GO ist und welcher Fader den Master steuert, ist
// eine Frage des Geräts und des Geschmacks, nicht des Projekts. Ein Profil fasst das zusammen; die Programmeinstellungen
// speichern es als Textzeile (serializeProfile/parseProfile), damit Studio und Tests dasselbe Format teilen.
//
// Gerät APC mini mk1 (Kanal 1): Pads sind die Noten 0..63, links unten 0, zeilenweise nach oben. Die Pad-LED wählt man
// über die Velocity eines Note-On (0 aus, 1 grün, 2 grün blinkend, 3 rot, 4 rot blinkend, 5 gelb, 6 gelb blinkend).
// Die Seitentasten rechts sind die Noten 82..89 (oben 82), die Fader CC 48..56 (Fader 9 = 56).
// Das Raster ist gedreht: Look i liegt in Zeile i / Spalten von oben, Spalte i % Spalten - wie im Live-Fenster.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace whitehole::midi {

enum class LiveDevice { ApcMiniMk1 };

/// Pad-Farben des APC mini mk1 (Velocity des Note-On).
enum class PadLed : std::uint8_t { Off = 0, Green = 1, GreenBlink = 2, Red = 3, RedBlink = 4, Yellow = 5, YellowBlink = 6 };

struct LiveControls {
    int goNote{-1};       // Note der Taste für GO; -1 = nicht belegt
    int stopNote{-1};     // Note der Taste "Live beenden"
    int masterCc{-1};     // Fader für die Master-Helligkeit
    int fadeCc{-1};       // Fader für die Überblendzeit (0..10 s)
    int crossfadeCc{-1};  // Fader für die manuelle Überblendung
};

struct LiveProfile {
    std::string name;
    LiveDevice device{LiveDevice::ApcMiniMk1};
    int columns{4};  // Looks je Zeile (1..8); 8 Zeilen
    LiveControls controls;
    bool builtin{false};  // vorgefertigt, schreibgeschützt (nicht gespeichert)

    /// Höchstzahl an Looks, die das Raster zeigt.
    [[nodiscard]] int capacity() const noexcept { return columns * kRows; }
    /// Pad-Note für Look `index`; leer, wenn er nicht ins Raster passt.
    [[nodiscard]] std::optional<std::uint8_t> noteForLook(int index) const noexcept;
    /// Look-Index für eine Pad-Note; leer für Noten außerhalb des Rasters (rechte Hälfte, Tasten).
    [[nodiscard]] std::optional<int> lookForNote(std::uint8_t note) const noexcept;

    static constexpr int kRows = 8;
    static constexpr int kPads = 64;
};

/// Die vorgefertigten Profile; das erste ist der Standard.
///   "Looks 4 x 8 mit GO und Fadern": 4 Spalten, Seitentaste 1 GO, Seitentaste 2 Live beenden, Fader 9 Master,
///   Fader 1 Überblendzeit, Fader 2 manuelle Überblendung.
///   "Looks 8 x 8 ohne Steuertasten": alle 64 Pads für Looks, Fader wie oben, keine Tasten.
[[nodiscard]] std::vector<LiveProfile> builtinLiveProfiles();

/// Eine Zeile Text: "name|apcmini1|spalten|go|stop|master|fade|crossfade" (Name ohne '|').
[[nodiscard]] std::string serializeProfile(const LiveProfile& profile);
/// Umkehrung von serializeProfile; leer bei kaputter Zeile oder Werten außerhalb des Bereichs.
[[nodiscard]] std::optional<LiveProfile> parseProfile(std::string_view line);

}  // namespace whitehole::midi
