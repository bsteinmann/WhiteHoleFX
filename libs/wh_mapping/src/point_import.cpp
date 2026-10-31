// point_import.cpp: Umsetzung zu point_import.hpp. CSV-Zeilen und JSON-Bäume werden zu Punkten, danach
// Spiegeln, Skalieren und Verschieben.
#include "whitehole/mapping/point_import.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <string>

#include "mini_json.hpp"
#include "whitehole/mapping/fixture.hpp"

namespace whitehole::mapping {

namespace {

Error bad(const std::string& message) { return Error{ErrorCode::InvalidArgument, message}; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.remove_suffix(1);
    }
    return s;
}

std::string lower(std::string_view s) {
    std::string out{s};
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool parseNumber(std::string_view token, bool decimalComma, double& out) {
    std::string text{trim(token)};
    if (decimalComma) {
        std::replace(text.begin(), text.end(), ',', '.');
    }
    if (!text.empty() && text.front() == '+') {
        text.erase(text.begin());
    }
    const auto r = std::from_chars(text.data(), text.data() + text.size(), out);
    return !text.empty() && r.ec == std::errc{} && r.ptr == text.data() + text.size() && std::isfinite(out);
}

std::vector<std::string_view> split(std::string_view line, char delimiter) {
    std::vector<std::string_view> tokens;
    if (delimiter == ' ') {  // Leerraum: beliebig viele Leerzeichen/Tabulatoren trennen
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                ++i;
            }
            const std::size_t start = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                ++i;
            }
            if (i > start) {
                tokens.push_back(line.substr(start, i - start));
            }
        }
        return tokens;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t at = line.find(delimiter, start);
        tokens.push_back(trim(line.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start)));
        if (at == std::string_view::npos) {
            return tokens;
        }
        start = at + 1;
    }
}

char detectDelimiter(std::string_view line) {
    for (const char c : {';', '\t', ','}) {
        if (line.find(c) != std::string_view::npos) {
            return c;
        }
    }
    return ' ';
}

/// Spiegeln, Skalieren, Verschieben und Prüfen der Obergrenze.
Result<std::vector<Vec3>> finish(std::vector<Vec3> points, const ImportOptions& options) {
    if (points.empty()) {
        return bad("Die Datei enthält keine Punkte");
    }
    if (points.size() > kMaxLedsPerFixture) {
        return bad("Zu viele Punkte (höchstens " + std::to_string(kMaxLedsPerFixture) + ")");
    }
    if (!(options.scale > 0.0F) || !std::isfinite(options.scale)) {
        return bad("Der Maßstab muss größer als 0 sein");
    }
    for (Vec3& p : points) {
        p.x *= options.scale;
        p.y *= options.flipY ? -options.scale : options.scale;
        p.z *= options.scale;
    }
    if (options.toOrigin) {
        Vec3 low = points.front();
        for (const Vec3& p : points) {
            low.x = std::min(low.x, p.x);
            low.y = std::min(low.y, p.y);
            low.z = std::min(low.z, p.z);
        }
        for (Vec3& p : points) {
            p.x -= low.x;
            p.y -= low.y;
            p.z -= low.z;
        }
    }
    return points;
}

}  // namespace

Result<std::vector<Vec3>> parsePointsCsv(std::string_view text, const ImportOptions& options) {
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") {
        text.remove_prefix(3);
    }
    std::vector<Vec3> points;
    char delimiter = ' ';
    bool decimalComma = false;
    bool first = true;
    int columnX = 0;
    int columnY = 1;
    int columnZ = -1;  // -1: Spalte 2 nur, wenn die Zeile drei Werte hat
    bool hasHeader = false;
    std::size_t lineNumber = 0;

    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = std::min(text.find('\n', pos), text.size());
        const std::string_view line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        ++lineNumber;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto where = [&](const std::string& message) { return bad("Zeile " + std::to_string(lineNumber) + ": " + message); };
        if (first) {
            first = false;
            delimiter = detectDelimiter(line);
            decimalComma = delimiter == ';' || delimiter == '\t';
            const std::vector<std::string_view> tokens = split(line, delimiter);
            double probe = 0.0;
            if (!parseNumber(tokens.front(), decimalComma, probe)) {  // Kopfzeile
                hasHeader = true;
                columnX = columnY = -1;
                for (std::size_t i = 0; i < tokens.size(); ++i) {
                    const std::string name = lower(tokens[i]);
                    if (name == "x") {
                        columnX = static_cast<int>(i);
                    } else if (name == "y") {
                        columnY = static_cast<int>(i);
                    } else if (name == "z") {
                        columnZ = static_cast<int>(i);
                    }
                }
                if (columnX < 0 || columnY < 0) {
                    return where("Die Kopfzeile braucht die Spalten x und y");
                }
                continue;
            }
        }
        const std::vector<std::string_view> tokens = split(line, delimiter);
        if (!hasHeader && (tokens.size() < 2 || tokens.size() > 3)) {
            return where("erwartet werden 2 oder 3 Werte (x, y[, z]), gefunden " + std::to_string(tokens.size()) +
                         "; mit Kopfzeile (x,y,z) sind weitere Spalten erlaubt");
        }
        const auto need = static_cast<std::size_t>(std::max({columnX, columnY, columnZ}) + 1);
        if (tokens.size() < need) {
            return where("zu wenige Spalten");
        }
        Vec3 p;
        double v = 0.0;
        if (!parseNumber(tokens[static_cast<std::size_t>(columnX)], decimalComma, v)) {
            return where("x ist keine Zahl: \"" + std::string{tokens[static_cast<std::size_t>(columnX)]} + "\"");
        }
        p.x = static_cast<float>(v);
        if (!parseNumber(tokens[static_cast<std::size_t>(columnY)], decimalComma, v)) {
            return where("y ist keine Zahl: \"" + std::string{tokens[static_cast<std::size_t>(columnY)]} + "\"");
        }
        p.y = static_cast<float>(v);
        const int zColumn = columnZ >= 0 ? columnZ : (!hasHeader && tokens.size() == 3 ? 2 : -1);
        if (zColumn >= 0) {
            if (!parseNumber(tokens[static_cast<std::size_t>(zColumn)], decimalComma, v)) {
                return where("z ist keine Zahl: \"" + std::string{tokens[static_cast<std::size_t>(zColumn)]} + "\"");
            }
            p.z = static_cast<float>(v);
        }
        points.push_back(p);
        if (points.size() > kMaxLedsPerFixture) {
            break;
        }
    }
    return finish(std::move(points), options);
}

namespace {

/// Eine Koordinate aus einem JSON-Zahlwert.
bool jsonNumber(const detail::Json* value, float& out) {
    if (value == nullptr || value->type != detail::Json::Type::Number) {
        return false;
    }
    out = static_cast<float>(value->number);
    return true;
}

/// Ein Punkt aus [x,y], [x,y,z] oder {"x":..,"y":..,"z":..}.
bool jsonPoint(const detail::Json& item, Vec3& out) {
    out = Vec3{};
    if (item.type == detail::Json::Type::Array) {
        if (item.items.size() < 2 || item.items.size() > 3) {
            return false;
        }
        bool ok = jsonNumber(&item.items[0], out.x) && jsonNumber(&item.items[1], out.y);
        if (ok && item.items.size() == 3) {
            ok = jsonNumber(&item.items[2], out.z);
        }
        return ok;
    }
    if (item.type == detail::Json::Type::Object) {
        const detail::Json* z = item.find("z");
        return jsonNumber(item.find("x"), out.x) && jsonNumber(item.find("y"), out.y) && (z == nullptr || jsonNumber(z, out.z));
    }
    return false;
}

}  // namespace

Result<std::vector<Vec3>> parsePointsJson(std::string_view text, const ImportOptions& options) {
    const Result<detail::Json> parsed = detail::parseJson(text);
    if (!parsed.ok()) {
        return parsed.error();
    }
    const detail::Json* list = &parsed.value();
    if (list->type == detail::Json::Type::Object) {
        list = nullptr;
        for (const char* key : {"points", "leds", "pixels", "positions"}) {
            if (const detail::Json* found = parsed.value().find(key); found != nullptr) {
                list = found;
                break;
            }
        }
        if (list == nullptr) {
            return bad("JSON: Im Objekt fehlt ein Feld \"points\", \"leds\", \"pixels\" oder \"positions\"");
        }
    }
    if (list->type != detail::Json::Type::Array) {
        return bad("JSON: Eine Liste von Punkten wird erwartet");
    }
    std::vector<Vec3> points;
    points.reserve(std::min<std::size_t>(list->items.size(), kMaxLedsPerFixture));
    for (std::size_t i = 0; i < list->items.size() && points.size() <= kMaxLedsPerFixture; ++i) {
        Vec3 p;
        if (!jsonPoint(list->items[i], p)) {
            return bad("JSON: Eintrag " + std::to_string(i + 1) + " ist kein Punkt ([x,y], [x,y,z] oder {\"x\":..,\"y\":..})");
        }
        points.push_back(p);
    }
    return finish(std::move(points), options);
}

Result<std::vector<Vec3>> parsePoints(std::string_view text, std::string_view extension, const ImportOptions& options) {
    const std::string ext = lower(extension);
    if (ext == ".csv" || ext == ".txt" || ext == ".tsv") {
        return parsePointsCsv(text, options);
    }
    if (ext == ".json") {
        return parsePointsJson(text, options);
    }
    return bad("Unbekanntes Format \"" + std::string{extension} + "\" (erwartet: .csv, .txt, .tsv oder .json)");
}

}  // namespace whitehole::mapping
