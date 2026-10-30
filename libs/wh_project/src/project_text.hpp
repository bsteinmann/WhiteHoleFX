// project_text.hpp (privat): Zerlegen der Zeilen in Wörter und Lesen von Zahlen.
//
// Warum es diese Datei gibt: Parser und Schreiber brauchen dieselben Regeln für Anführungs-
// zeichen und Zahlen. Hier stehen sie einmal. Ein `Reader` liest die Wörter einer Zeile und
// merkt sich den ersten Fehler; der Aufrufer prüft nach einer Anweisung `ok()`. Das spart
// nach jedem Wort eine Fehlerabfrage.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"

namespace whitehole::project::detail {

struct Line {
    std::size_t number{0};  // 1-basiert
    std::vector<std::string> words;
};

/// Zerlegt den Text in Zeilen mit Wörtern. Leere und Kommentarzeilen entfallen.
/// Anführungszeichen fassen ein Wort zusammen; darin gelten \" \\ \n.
/// Fehler: InvalidArgument (nicht geschlossenes Anführungszeichen, unbekannte Maskierung).
[[nodiscard]] Result<std::vector<Line>> splitLines(std::string_view text);

/// Wort für die Datei: in Anführungszeichen, wenn nötig.
[[nodiscard]] std::string quoteWord(std::string_view word);

[[nodiscard]] std::string formatFloat(float value);
[[nodiscard]] std::string formatDouble(double value);

class Reader {
public:
    Reader(const Line& line, std::size_t first) : line_{line}, next_{first} {}

    [[nodiscard]] bool atEnd() const noexcept { return next_ >= line_.words.size(); }
    [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
    [[nodiscard]] const Error& error() const { return *error_; }
    [[nodiscard]] std::size_t line() const noexcept { return line_.number; }

    void fail(std::string message, ErrorCode code = ErrorCode::InvalidArgument);

    /// Das nächste Wort. Am Zeilenende: Fehler "<what> fehlt".
    std::string word(std::string_view what);
    float number(std::string_view what);
    double numberD(std::string_view what);
    /// Ganzzahl 0..maxValue.
    std::uint32_t whole(std::string_view what, std::uint32_t maxValue);
    std::int64_t integer(std::string_view what);
    bool boolean(std::string_view what);
    Vec2 vec2(std::string_view what);
    Vec3 vec3(std::string_view what);
    ColorRgba color(std::string_view what);

    /// Fehler, wenn noch Wörter übrig sind.
    void expectEnd();

private:
    const Line& line_;
    std::size_t next_;
    std::optional<Error> error_;
};

template <typename E, std::size_t N>
using NameTable = std::array<std::pair<std::string_view, E>, N>;

template <typename E, std::size_t N>
[[nodiscard]] std::optional<E> lookupName(const NameTable<E, N>& table, std::string_view word) {
    for (const auto& [name, value] : table) {
        if (name == word) {
            return value;
        }
    }
    return std::nullopt;
}

template <typename E, std::size_t N>
[[nodiscard]] std::string_view nameOf(const NameTable<E, N>& table, E value) {
    for (const auto& [name, candidate] : table) {
        if (candidate == value) {
            return name;
        }
    }
    return "?";
}

}  // namespace whitehole::project::detail
