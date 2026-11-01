// po_catalog.cpp: .po-Dateien lesen, Platzhalter vergleichen, Texte für .po escapen.
#include "whitehole/i18n/po_catalog.hpp"

#include <algorithm>
#include <cctype>

namespace whitehole::i18n {

namespace {

Error parseError(std::size_t line, const std::string& what) {
    return Error{ErrorCode::InvalidArgument, "Zeile " + std::to_string(line) + ": " + what};
}

/// Der Inhalt einer Zeile der Form "…" ohne Anführungszeichen, mit aufgelösten Escapes.
Result<std::string> unquote(std::string_view line, std::size_t number) {
    if (line.size() < 2 || line.front() != '"' || line.back() != '"') {
        return parseError(number, "Zeichenkette in Anführungszeichen erwartet");
    }
    std::string out;
    for (std::size_t i = 1; i + 1 < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            return parseError(number, "nicht maskiertes Anführungszeichen");
        }
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (++i + 1 > line.size() - 1) {
            return parseError(number, "Escape am Zeilenende");
        }
        switch (line[i]) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            default: return parseError(number, std::string{"unbekannte Escape-Folge \\"} + line[i]);
        }
    }
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.remove_suffix(1);
    }
    return s;
}

bool startsWith(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }

std::vector<std::string> placeholders(std::string_view text) {
    std::vector<std::string> found;
    for (std::size_t i = 0; i + 1 < text.size(); ++i) {
        if (text[i] != '%') {
            continue;
        }
        std::size_t j = i + 1;
        if (text[j] == 'n') {
            found.emplace_back("%n");
        } else if (std::isdigit(static_cast<unsigned char>(text[j])) != 0) {
            while (j < text.size() && j < i + 3 && std::isdigit(static_cast<unsigned char>(text[j])) != 0) {
                ++j;
            }
            found.emplace_back(text.substr(i, j - i));
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

}  // namespace

Result<PoCatalog> PoCatalog::parse(std::string_view text) {
    PoCatalog catalog;
    enum class Field { None, Context, Id, Str } field = Field::None;
    std::string id;
    std::string str;
    bool haveId = false;
    bool haveStr = false;
    bool fuzzy = false;

    const auto flush = [&]() {
        if (haveId && !id.empty()) {  // msgid "" ist der Kopf der Datei
            catalog.sources_.push_back(id);
            if (!fuzzy && !str.empty()) {
                catalog.translated_[id] = str;
            } else {
                catalog.untranslated_.push_back(id);
            }
        }
        id.clear();
        str.clear();
        haveId = haveStr = fuzzy = false;
        field = Field::None;
    };

    std::size_t number = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = std::min(text.find('\n', pos), text.size());
        std::string_view line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        ++number;

        if (line.empty()) {
            if (haveId && !haveStr) {
                return parseError(number, "msgid ohne msgstr");
            }
            flush();
            continue;
        }
        if (line.front() == '#') {
            if (startsWith(line, "#~")) {
                continue;  // veralteter Eintrag
            }
            if (haveStr) {
                flush();
            }
            if (startsWith(line, "#,") && line.find("fuzzy") != std::string_view::npos) {
                fuzzy = true;
            }
            continue;
        }
        if (startsWith(line, "msgctxt")) {
            if (haveStr) {
                flush();
            }
            field = Field::Context;
            continue;  // der Kontext wird nicht ausgewertet
        }
        if (startsWith(line, "msgid_plural") || startsWith(line, "msgstr[")) {
            return parseError(number, "Pluralformen werden nicht unterstützt");
        }
        if (startsWith(line, "msgid")) {
            if (haveStr) {
                flush();
            } else if (haveId) {
                return parseError(number, "zwei msgid hintereinander");
            }
            auto value = unquote(trim(line.substr(5)), number);
            if (!value.ok()) {
                return value.error();
            }
            id = std::move(value).value();
            haveId = true;
            field = Field::Id;
            continue;
        }
        if (startsWith(line, "msgstr")) {
            if (!haveId || haveStr) {
                return parseError(number, "msgstr ohne passendes msgid");
            }
            auto value = unquote(trim(line.substr(6)), number);
            if (!value.ok()) {
                return value.error();
            }
            str = std::move(value).value();
            haveStr = true;
            field = Field::Str;
            continue;
        }
        if (line.front() == '"') {
            auto value = unquote(line, number);
            if (!value.ok()) {
                return value.error();
            }
            switch (field) {
                case Field::Id: id += value.value(); break;
                case Field::Str: str += value.value(); break;
                case Field::Context: break;
                case Field::None: return parseError(number, "Fortsetzungszeile ohne vorangehendes Schlüsselwort");
            }
            continue;
        }
        return parseError(number, "unbekanntes Schlüsselwort");
    }
    if (haveId && !haveStr) {
        return parseError(number, "msgid ohne msgstr am Dateiende");
    }
    flush();
    return catalog;
}

const std::string* PoCatalog::find(std::string_view source) const {
    const auto it = translated_.find(std::string{source});
    return it == translated_.end() ? nullptr : &it->second;
}

bool placeholdersMatch(std::string_view source, std::string_view translation) {
    return placeholders(source) == placeholders(translation);
}

std::string escapePo(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

}  // namespace whitehole::i18n
