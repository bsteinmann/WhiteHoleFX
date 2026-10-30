// project_text.cpp: Wörter, Anführungszeichen und Zahlen der Projektdatei.
#include "project_text.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

namespace {

Error lineError(std::size_t number, const std::string& message) {
    return Error{ErrorCode::InvalidArgument, "Zeile " + std::to_string(number) + ": " + message};
}

bool isSpace(char c) noexcept { return c == ' ' || c == '\t' || c == '\r'; }

}  // namespace

Result<std::vector<Line>> splitLines(std::string_view text) {
    std::vector<Line> lines;
    std::size_t number = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = std::min(text.find('\n', pos), text.size());
        const std::string_view raw = text.substr(pos, end - pos);
        pos = end + 1;
        ++number;

        Line line;
        line.number = number;
        std::size_t i = 0;
        while (i < raw.size()) {
            if (isSpace(raw[i])) {
                ++i;
            } else if (raw[i] == '#') {
                break;
            } else if (raw[i] == '"') {
                std::string word;
                ++i;
                bool closed = false;
                while (i < raw.size()) {
                    const char c = raw[i++];
                    if (c == '"') {
                        closed = true;
                        break;
                    }
                    if (c != '\\') {
                        word.push_back(c);
                        continue;
                    }
                    if (i >= raw.size()) {
                        break;
                    }
                    const char escaped = raw[i++];
                    if (escaped == '"' || escaped == '\\') {
                        word.push_back(escaped);
                    } else if (escaped == 'n') {
                        word.push_back('\n');
                    } else {
                        return lineError(number, std::string{"Unbekannte Maskierung \\"} + escaped);
                    }
                }
                if (!closed) {
                    return lineError(number, "Anführungszeichen nicht geschlossen");
                }
                line.words.push_back(std::move(word));
            } else {
                const std::size_t start = i;
                while (i < raw.size() && !isSpace(raw[i]) && raw[i] != '"') {
                    ++i;
                }
                line.words.emplace_back(raw.substr(start, i - start));
            }
        }
        if (!line.words.empty()) {
            lines.push_back(std::move(line));
        }
    }
    return lines;
}

std::string quoteWord(std::string_view word) {
    bool plain = !word.empty();
    for (const char c : word) {
        if (isSpace(c) || c == '"' || c == '#' || c == '\\' || c == '\n') {
            plain = false;
        }
    }
    if (plain) {
        return std::string{word};
    }
    std::string out = "\"";
    for (const char c : word) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

namespace {

template <typename T>
std::string formatShortest(T value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    return std::string{buffer, result.ptr};
}

template <typename T>
std::optional<T> parseNumber(const std::string& word) {
    T value{};
    const char* const first = word.data();
    const char* const last = first + word.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
    }
    return value;
}

}  // namespace

std::string formatFloat(float value) { return formatShortest(value); }
std::string formatDouble(double value) { return formatShortest(value); }

void Reader::fail(std::string message, ErrorCode code) {
    if (!error_) {
        error_ = Error{code, "Zeile " + std::to_string(line_.number) + ": " + std::move(message)};
    }
}

std::string Reader::word(std::string_view what) {
    if (atEnd()) {
        fail(std::string{what} + " fehlt");
        return {};
    }
    return line_.words[next_++];
}

float Reader::number(std::string_view what) {
    const std::string w = word(what);
    if (!ok()) {
        return 0.0F;
    }
    if (const auto v = parseNumber<float>(w)) {
        return *v;
    }
    fail(std::string{what} + ": \"" + w + "\" ist keine endliche Zahl");
    return 0.0F;
}

double Reader::numberD(std::string_view what) {
    const std::string w = word(what);
    if (!ok()) {
        return 0.0;
    }
    if (const auto v = parseNumber<double>(w)) {
        return *v;
    }
    fail(std::string{what} + ": \"" + w + "\" ist keine endliche Zahl");
    return 0.0;
}

std::uint32_t Reader::whole(std::string_view what, std::uint32_t maxValue) {
    const std::string w = word(what);
    if (!ok()) {
        return 0;
    }
    const auto v = parseNumber<std::uint64_t>(w);
    if (!v || *v > maxValue) {
        fail(std::string{what} + ": \"" + w + "\" ist keine ganze Zahl von 0 bis " +
             std::to_string(maxValue));
        return 0;
    }
    return static_cast<std::uint32_t>(*v);
}

std::int64_t Reader::integer(std::string_view what) {
    const std::string w = word(what);
    if (!ok()) {
        return 0;
    }
    if (const auto v = parseNumber<std::int64_t>(w)) {
        return *v;
    }
    fail(std::string{what} + ": \"" + w + "\" ist keine ganze Zahl");
    return 0;
}

bool Reader::boolean(std::string_view what) {
    const std::string w = word(what);
    if (ok() && w != "true" && w != "false") {
        fail(std::string{what} + ": erwartet true oder false, gefunden \"" + w + "\"");
    }
    return w == "true";
}

Vec2 Reader::vec2(std::string_view what) {
    Vec2 v;
    v.x = number(what);
    v.y = number(what);
    return v;
}

Vec3 Reader::vec3(std::string_view what) {
    Vec3 v;
    v.x = number(what);
    v.y = number(what);
    v.z = number(what);
    return v;
}

ColorRgba Reader::color(std::string_view what) {
    ColorRgba c;
    c.r = number(what);
    c.g = number(what);
    c.b = number(what);
    c.a = number(what);
    return c;
}

void Reader::expectEnd() {
    if (ok() && !atEnd()) {
        fail("Unerwartetes Wort \"" + line_.words[next_] + "\"");
    }
}

}  // namespace whitehole::project::detail

namespace whitehole::project {

Result<std::string> readTextFile(const std::string& path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        return Error{ErrorCode::IoError, "Datei kann nicht geöffnet werden: " + path};
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (in.bad()) {
        return Error{ErrorCode::IoError, "Lesefehler: " + path};
    }
    return buffer.str();
}

Result<void> writeTextFile(const std::string& path, std::string_view text) {
    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    if (!out) {
        return Error{ErrorCode::IoError, "Datei kann nicht geschrieben werden: " + path};
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    if (!out) {
        return Error{ErrorCode::IoError, "Schreibfehler: " + path};
    }
    return {};
}

}  // namespace whitehole::project
