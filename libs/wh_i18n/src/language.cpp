// language.cpp: Sprachcodes prüfen und aus dem Gebietsschema ableiten.
#include "whitehole/i18n/language.hpp"

#include <algorithm>
#include <cctype>

namespace whitehole::i18n {

bool isKnownLanguage(std::string_view code) noexcept {
    return std::any_of(kLanguages.begin(), kLanguages.end(), [code](const Language& l) { return l.code == code; });
}

std::string languageFromLocale(std::string_view locale) {
    std::string lower;
    for (const char c : locale) {
        if (c == '_' || c == '-' || c == '.' || c == '@') {
            break;  // nur der Sprachteil zählt: "fr_FR.UTF-8" -> "fr"
        }
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return isKnownLanguage(lower) ? lower : std::string{"en"};
}

}  // namespace whitehole::i18n
