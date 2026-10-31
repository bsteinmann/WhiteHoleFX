// snippet_expander.cpp: Umsetzung zu snippet_expander.hpp. Ersetzt die Platzhalter eines Snippet-Rumpfs durch WGSL-Ausdrücke.
#include "whitehole/compiler/snippet_expander.hpp"

#include <cctype>
#include <cstddef>

namespace whitehole::compiler {

namespace {

Error invalid(const std::string& message) {
    return Error{ErrorCode::InvalidArgument, "Snippet: " + message};
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

/// Position der schließenden Klammer zu der öffnenden an `open`, oder npos.
std::size_t findClosingParen(std::string_view text, std::size_t open) {
    int depth = 0;
    for (std::size_t i = open; i < text.size(); ++i) {
        if (text[i] == '(') {
            ++depth;
        } else if (text[i] == ')' && --depth == 0) {
            return i;
        }
    }
    return std::string_view::npos;
}

/// Position des ersten Kommas auf Klammertiefe 0, oder npos.
std::size_t findTopLevelComma(std::string_view text) {
    int depth = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '(') {
            ++depth;
        } else if (text[i] == ')') {
            --depth;
        } else if (text[i] == ',' && depth == 0) {
            return i;
        }
    }
    return std::string_view::npos;
}

}  // namespace

Result<std::string> expandSnippet(std::string_view body, const SnippetBindings& bindings) {
    std::string result;
    result.reserve(body.size());

    std::size_t i = 0;
    while (i < body.size()) {
        if (body[i] != '@') {
            result.push_back(body[i++]);
            continue;
        }

        std::size_t nameEnd = i + 1;
        while (nameEnd < body.size() &&
               (std::isalpha(static_cast<unsigned char>(body[nameEnd])) != 0 || body[nameEnd] == '_')) {
            ++nameEnd;
        }
        const std::string_view name = body.substr(i + 1, nameEnd - i - 1);

        if (name == "time") {
            result += bindings.time;
            i = nameEnd;
            continue;
        }
        if (name != "in" && name != "param") {
            return invalid("unbekannter Platzhalter '@" + std::string{name} + "'");
        }
        if (nameEnd >= body.size() || body[nameEnd] != '(') {
            return invalid("nach '@" + std::string{name} + "' fehlt '('");
        }
        const std::size_t close = findClosingParen(body, nameEnd);
        if (close == std::string_view::npos) {
            return invalid("'@" + std::string{name} + "(' wird nicht geschlossen");
        }
        const std::string_view args = body.substr(nameEnd + 1, close - nameEnd - 1);

        if (name == "param") {
            const std::string_view key = trim(args);
            if (key.empty()) {
                return invalid("@param ohne Schlüssel");
            }
            Result<std::string> value = bindings.parameter(key);
            if (!value.ok()) {
                return value;
            }
            result += value.value();
        } else {
            const std::size_t comma = findTopLevelComma(args);
            if (comma == std::string_view::npos) {
                return invalid("@in braucht Port und Koordinate, getrennt durch ein Komma");
            }
            const std::string_view port = trim(args.substr(0, comma));
            if (port.empty()) {
                return invalid("@in ohne Port");
            }
            Result<std::string> coordinate = expandSnippet(trim(args.substr(comma + 1)), bindings);
            if (!coordinate.ok()) {
                return coordinate;
            }
            Result<std::string> call = bindings.input(port, coordinate.value());
            if (!call.ok()) {
                return call;
            }
            result += call.value();
        }
        i = close + 1;
    }
    return result;
}

}  // namespace whitehole::compiler
