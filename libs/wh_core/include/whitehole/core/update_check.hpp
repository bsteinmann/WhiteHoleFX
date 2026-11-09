// update_check.hpp: Versionsnummern vergleichen und die Antwort von GitHub ("neuestes Release") lesen.
//
// Warum es diese Datei gibt: Die Update-Benachrichtigung der Oberfläche soll sich ohne Netzwerk und ohne Qt
// prüfen lassen. Hier steht deshalb nur die reine Logik: Was ist eine Version, welche ist neuer, und welche
// Felder braucht man aus dem JSON der GitHub-API (GET /repos/<Besitzer>/<Repo>/releases/latest). Das Abrufen
// selbst (Netzwerk, Einwilligung, Dialoge) macht die Oberfläche (apps/whitehole_studio/update_checker.*).
//
// Der JSON-Leser ist absichtlich klein: Er liest nur Felder der obersten Ebene (tag_name, html_url, draft,
// prerelease). Gleichnamige Felder in verschachtelten Objekten ("author" hat ebenfalls ein html_url) zählen
// nicht. Der Link muss mit https://github.com/ beginnen, sonst wird die Antwort verworfen: Das Programm öffnet
// nie eine Adresse, die ein Server frei vorgibt.
// Abhängigkeiten: keine.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace whitehole::core {

struct Version {
    int major{0};
    int minor{0};
    int patch{0};

    auto operator<=>(const Version&) const = default;
};

namespace update_detail {

inline bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

/// Liest eine Zahl ab `pos`; scheitert bei fehlenden Ziffern und bei Werten über 999999.
inline std::optional<int> readNumber(std::string_view text, std::size_t& pos) {
    if (pos >= text.size() || !isDigit(text[pos])) {
        return std::nullopt;
    }
    int value = 0;
    while (pos < text.size() && isDigit(text[pos])) {
        value = value * 10 + (text[pos] - '0');
        if (value > 999999) {
            return std::nullopt;
        }
        ++pos;
    }
    return value;
}

/// `pos` zeigt auf das öffnende Anführungszeichen; danach hinter dem schließenden. Versteht \" \\ \/ \n \t,
/// andere Folgen (auch \uXXXX) bleiben unverändert im Ergebnis (für die gesuchten Felder ohne Bedeutung).
inline std::optional<std::string> readString(std::string_view text, std::size_t& pos) {
    std::string out;
    ++pos;
    while (pos < text.size()) {
        const char c = text[pos++];
        if (c == '"') {
            return out;
        }
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (pos >= text.size()) {
            break;
        }
        const char e = text[pos++];
        switch (e) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case '"': case '\\': case '/': out.push_back(e); break;
            default: out.push_back('\\'); out.push_back(e); break;
        }
    }
    return std::nullopt;  // Zeichenkette nicht beendet
}

inline void skipSpace(std::string_view text, std::size_t& pos) noexcept {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r')) {
        ++pos;
    }
}

}  // namespace update_detail

/// "v0.65.0", "0.65.0", "0.65.0-1" (Paketnummer) oder "0.65" -> Version. Ein Rest hinter der dritten Zahl
/// muss mit '-' oder '+' beginnen; alles andere ("0.65.0x", "abc") ergibt nullopt.
[[nodiscard]] inline std::optional<Version> parseVersion(std::string_view text) {
    std::size_t pos = 0;
    if (pos < text.size() && (text[pos] == 'v' || text[pos] == 'V')) {
        ++pos;
    }
    Version version;
    const auto major = update_detail::readNumber(text, pos);
    if (!major) {
        return std::nullopt;
    }
    version.major = *major;
    int* const rest[] = {&version.minor, &version.patch};
    for (int* slot : rest) {
        if (pos >= text.size() || text[pos] != '.') {
            break;
        }
        ++pos;
        const auto number = update_detail::readNumber(text, pos);
        if (!number) {
            return std::nullopt;
        }
        *slot = *number;
    }
    if (pos < text.size() && text[pos] != '-' && text[pos] != '+') {
        return std::nullopt;
    }
    return version;
}

[[nodiscard]] inline std::string toString(const Version& v) {
    return std::to_string(v.major) + '.' + std::to_string(v.minor) + '.' + std::to_string(v.patch);
}

struct ReleaseInfo {
    std::string tag;  // wie bei GitHub, z. B. "v0.65.0"
    std::string url;  // Seite des Releases im Browser
    Version version;
};

/// Liest die Antwort von releases/latest. Ergibt nullopt bei ungültigem JSON, fehlendem tag_name/html_url,
/// unlesbarer Versionsnummer, einem Link außerhalb von https://github.com/ sowie bei Entwürfen und
/// Vorabversionen (draft/prerelease = true).
[[nodiscard]] inline std::optional<ReleaseInfo> parseLatestRelease(std::string_view json) {
    using namespace update_detail;
    std::size_t pos = 0;
    skipSpace(json, pos);
    if (pos >= json.size() || json[pos] != '{') {
        return std::nullopt;
    }
    std::string tag;
    std::string url;
    bool preliminary = false;
    int depth = 0;
    while (pos < json.size()) {
        const char c = json[pos];
        if (c == '{' || c == '[') {
            ++depth;
            ++pos;
        } else if (c == '}' || c == ']') {
            --depth;
            ++pos;
            if (depth <= 0) {
                break;
            }
        } else if (c == '"') {
            auto key = readString(json, pos);
            if (!key) {
                return std::nullopt;
            }
            if (depth != 1) {
                continue;  // Text in einem verschachtelten Wert
            }
            // Auf der obersten Ebene ist jede Zeichenkette, die hier ankommt, ein Schlüssel.
            skipSpace(json, pos);
            if (pos >= json.size() || json[pos] != ':') {
                return std::nullopt;
            }
            ++pos;
            skipSpace(json, pos);
            if (pos < json.size() && json[pos] == '"') {
                auto value = readString(json, pos);
                if (!value) {
                    return std::nullopt;
                }
                if (*key == "tag_name") {
                    tag = std::move(*value);
                } else if (*key == "html_url") {
                    url = std::move(*value);
                }
            } else if (*key == "draft" || *key == "prerelease") {
                preliminary = preliminary || json.substr(pos, 4) == "true";
            }
        } else {
            ++pos;
        }
    }
    constexpr std::string_view kHost = "https://github.com/";
    if (preliminary || tag.empty() || url.compare(0, kHost.size(), kHost) != 0) {
        return std::nullopt;
    }
    const auto version = parseVersion(tag);
    if (!version) {
        return std::nullopt;
    }
    return ReleaseInfo{std::move(tag), std::move(url), *version};
}

}  // namespace whitehole::core
