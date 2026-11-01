// language.hpp: die Sprachen der Oberfläche und die Wahl der Startsprache.
//
// Warum es das gibt: Welche Sprachen es gibt, soll an einer Stelle stehen (Menü, Einstellungen, Kataloge,
// Tests). Quelltext und Standard sind Deutsch; Englisch, Französisch und Spanisch kommen aus Katalogen.
#pragma once

#include <array>
#include <string>
#include <string_view>

namespace whitehole::i18n {

struct Language {
    std::string_view code;        // "de", "en", "fr", "es"
    std::string_view nativeName;  // im eigenen Namen, damit man seine Sprache auch erkennt, wenn man die Oberfläche nicht lesen kann
};

inline constexpr std::array<Language, 4> kLanguages{{
    {"de", "Deutsch"},
    {"en", "English"},
    {"fr", "Français"},
    {"es", "Español"},
}};

inline constexpr std::string_view kSourceLanguage = "de";  // die Sprache der Texte im Quelltext

/// Ist das ein bekannter Sprachcode (genau, kleingeschrieben)?
[[nodiscard]] bool isKnownLanguage(std::string_view code) noexcept;

/// Aus einem Gebietsschema wie "fr_FR.UTF-8", "es-MX", "de" oder "C" die passende Sprache wählen.
/// Unbekannte Sprachen ergeben "en", weil Englisch die verbreitetste Ausweichsprache ist.
[[nodiscard]] std::string languageFromLocale(std::string_view locale);

}  // namespace whitehole::i18n
