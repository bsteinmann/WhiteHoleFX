// mini_json.cpp: Umsetzung zu mini_json.hpp. Rekursiver Abstieg über den Text.
#include "mini_json.hpp"

#include <charconv>
#include <cmath>

namespace whitehole::mapping::detail {

const Json* Json::find(std::string_view key) const {
    for (const auto& field : fields) {
        if (field.first == key) {
            return &field.second;
        }
    }
    return nullptr;
}

namespace {

constexpr int kMaxDepth = 32;

class Reader {
public:
    explicit Reader(std::string_view text) : text_{text} {}

    Result<Json> run() {
        skipSpace();
        Json value;
        if (Result<void> r = parseValue(value, 0); !r.ok()) {
            return r.error();
        }
        skipSpace();
        if (pos_ != text_.size()) {
            return fail("Nach dem JSON-Wert steht noch Text");
        }
        return value;
    }

private:
    Error fail(const std::string& message) const {
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            if (text_[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        return Error{ErrorCode::InvalidArgument,
                     "JSON, Zeile " + std::to_string(line) + ", Spalte " + std::to_string(column) + ": " + message};
    }

    void skipSpace() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool take(char c) {
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    Result<void> parseValue(Json& out, int depth) {
        if (depth > kMaxDepth) {
            return fail("zu tief verschachtelt");
        }
        if (pos_ >= text_.size()) {
            return fail("Wert erwartet, Datei zu Ende");
        }
        const char c = text_[pos_];
        if (c == '[') {
            return parseArray(out, depth);
        }
        if (c == '{') {
            return parseObject(out, depth);
        }
        if (c == '"') {
            out.type = Json::Type::String;
            return parseString(out.text);
        }
        if (literal("true")) {
            out.type = Json::Type::Bool;
            out.boolean = true;
            return {};
        }
        if (literal("false")) {
            out.type = Json::Type::Bool;
            return {};
        }
        if (literal("null")) {
            return {};
        }
        return parseNumber(out);
    }

    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) == word) {
            pos_ += word.size();
            return true;
        }
        return false;
    }

    Result<void> parseNumber(Json& out) {
        const std::size_t start = pos_;
        while (pos_ < text_.size() && std::string_view{"+-0123456789.eE"}.find(text_[pos_]) != std::string_view::npos) {
            ++pos_;
        }
        double value = 0.0;
        const auto r = std::from_chars(text_.data() + start, text_.data() + pos_, value);
        if (start == pos_ || r.ec != std::errc{} || r.ptr != text_.data() + pos_ || !std::isfinite(value)) {
            pos_ = start;
            return fail("Zahl erwartet");
        }
        out.type = Json::Type::Number;
        out.number = value;
        return {};
    }

    Result<void> parseString(std::string& out) {
        ++pos_;  // öffnendes Anführungszeichen
        while (pos_ < text_.size() && text_[pos_] != '"') {
            char c = text_[pos_++];
            if (c == '\\' && pos_ < text_.size()) {
                const char e = text_[pos_++];
                c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;  // \uXXXX wird nicht gebraucht und als Zeichen übernommen
            }
            out.push_back(c);
        }
        if (!take('"')) {
            return fail("Text nicht beendet");
        }
        return {};
    }

    Result<void> parseArray(Json& out, int depth) {
        out.type = Json::Type::Array;
        ++pos_;
        skipSpace();
        if (take(']')) {
            return {};
        }
        while (true) {
            skipSpace();
            Json item;
            if (Result<void> r = parseValue(item, depth + 1); !r.ok()) {
                return r;
            }
            out.items.push_back(std::move(item));
            skipSpace();
            if (take(',')) {
                continue;
            }
            if (take(']')) {
                return {};
            }
            return fail("',' oder ']' erwartet");
        }
    }

    Result<void> parseObject(Json& out, int depth) {
        out.type = Json::Type::Object;
        ++pos_;
        skipSpace();
        if (take('}')) {
            return {};
        }
        while (true) {
            skipSpace();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return fail("Feldname in Anführungszeichen erwartet");
            }
            std::string key;
            if (Result<void> r = parseString(key); !r.ok()) {
                return r;
            }
            skipSpace();
            if (!take(':')) {
                return fail("':' erwartet");
            }
            skipSpace();
            Json value;
            if (Result<void> r = parseValue(value, depth + 1); !r.ok()) {
                return r;
            }
            out.fields.emplace_back(std::move(key), std::move(value));
            skipSpace();
            if (take(',')) {
                continue;
            }
            if (take('}')) {
                return {};
            }
            return fail("',' oder '}' erwartet");
        }
    }

    std::string_view text_;
    std::size_t pos_{0};
};

}  // namespace

Result<Json> parseJson(std::string_view text) {
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") {
        text.remove_prefix(3);  // Byte-Order-Mark
    }
    return Reader{text}.run();
}

}  // namespace whitehole::mapping::detail
