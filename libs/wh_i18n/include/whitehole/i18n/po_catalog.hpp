// po_catalog.hpp: ein Übersetzungskatalog im gettext-Format (.po / .pot).
//
// Warum es das gibt: Die Oberfläche hat deutsche Texte im Quelltext. Für andere Sprachen liegt je Sprache
// eine .po-Datei vor (msgid = deutscher Quelltext, msgstr = Übersetzung). Dieser Leser versteht den
// Teil von gettext, den wir brauchen: msgid/msgstr (auch über mehrere Zeilen), Escapes (\n \t \" \\),
// Kommentare, msgctxt (wird übergangen: gleicher Text, gleiche Übersetzung). Fuzzy-Einträge zählen nicht.
// Einträge mit leerem msgstr gelten als "noch nicht übersetzt" und fehlen im Katalog; der Aufrufer fällt
// dann auf den deutschen Text zurück.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::i18n {

class PoCatalog {
public:
    /// Liest einen Katalog. Fehler (kaputte Anführungszeichen, msgstr ohne msgid) nennen die Zeilennummer.
    [[nodiscard]] static Result<PoCatalog> parse(std::string_view text);

    /// Die Übersetzung zu einem deutschen Text oder nullptr.
    [[nodiscard]] const std::string* find(std::string_view source) const;

    /// Alle msgid, auch untranslated (für die Vollständigkeitsprüfung).
    [[nodiscard]] const std::vector<std::string>& sources() const noexcept { return sources_; }
    /// Anzahl der Einträge mit nicht-leerem msgstr.
    [[nodiscard]] std::size_t translatedCount() const noexcept { return translated_.size(); }
    /// msgid ohne Übersetzung (leeres msgstr), Reihenfolge wie in der Datei.
    [[nodiscard]] const std::vector<std::string>& untranslated() const noexcept { return untranslated_; }

private:
    std::unordered_map<std::string, std::string> translated_;
    std::vector<std::string> sources_;
    std::vector<std::string> untranslated_;
};

/// Sind in Quelle und Übersetzung dieselben Platzhalter (%1 … %99, %n) enthalten, jeder gleich oft?
[[nodiscard]] bool placeholdersMatch(std::string_view source, std::string_view translation);

/// Schreibt einen Text als Inhalt einer PO-Zeichenkette (mit Escapes, ohne die äußeren Anführungszeichen).
[[nodiscard]] std::string escapePo(std::string_view text);

}  // namespace whitehole::i18n
