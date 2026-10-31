// wgsl_bindings.cpp: Umsetzung zu wgsl_bindings.hpp. Liest aus WGSL-Text, welche Ressourcen ein Shader deklariert.
#include "whitehole/gpu/wgsl_bindings.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

namespace whitehole::gpu {

namespace {

Error bad(std::string message) {
    return Error{ErrorCode::ShaderError, std::move(message)};
}

/// Ersetzt // und /* */ Kommentare durch Leerzeichen (Zeilenumbrüche bleiben).
std::string stripComments(std::string_view text) {
    std::string out{text};
    std::size_t i = 0;
    while (i < out.size()) {
        if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '/') {
            while (i < out.size() && out[i] != '\n') {
                out[i++] = ' ';
            }
        } else if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '*') {
            std::size_t end = out.find("*/", i + 2);
            end = (end == std::string::npos) ? out.size() : end + 2;
            for (; i < end; ++i) {
                if (out[i] != '\n') {
                    out[i] = ' ';
                }
            }
        } else {
            ++i;
        }
    }
    return out;
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

class Cursor {
public:
    explicit Cursor(const std::string& text, std::size_t pos) : text_{text}, pos_{pos} {}

    void skipSpace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool consume(char c) {
        skipSpace();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool consumeWord(std::string_view word) {
        skipSpace();
        if (text_.compare(pos_, word.size(), word) != 0) {
            return false;
        }
        const std::size_t end = pos_ + word.size();
        if (end < text_.size() && isIdentChar(text_[end]) && isIdentChar(word.back())) {
            return false;  // "variable" ist nicht "var"
        }
        pos_ = end;
        return true;
    }

    std::optional<std::uint32_t> number() {
        skipSpace();
        std::size_t digits = 0;
        std::uint32_t value = 0;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
            if (++digits > 6) {
                return std::nullopt;
            }
            value = value * 10U + static_cast<std::uint32_t>(text_[pos_] - '0');
            ++pos_;
        }
        return digits == 0 ? std::nullopt : std::optional<std::uint32_t>{value};
    }

    std::string identifier() {
        skipSpace();
        const std::size_t start = pos_;
        while (pos_ < text_.size() && isIdentChar(text_[pos_])) {
            ++pos_;
        }
        return text_.substr(start, pos_ - start);
    }

    /// Text bis zum nächsten `stop` (ohne dieses), getrimmt. Die Position steht danach auf `stop`.
    std::optional<std::string> until(char stop) {
        const std::size_t end = text_.find(stop, pos_);
        if (end == std::string::npos) {
            return std::nullopt;
        }
        std::string part = text_.substr(pos_, end - pos_);
        pos_ = end;
        return trim(part);
    }

    [[nodiscard]] std::size_t position() const { return pos_; }

    static std::string trim(const std::string& s) {
        const auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
        const auto first = std::find_if(s.begin(), s.end(), notSpace);
        const auto last = std::find_if(s.rbegin(), s.rend(), notSpace).base();
        return first < last ? std::string(first, last) : std::string{};
    }

private:
    const std::string& text_;
    std::size_t pos_;
};

std::string squeeze(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (std::isspace(static_cast<unsigned char>(c)) == 0) {
            out += c;
        }
    }
    return out;
}

std::optional<TextureFormat> formatFromName(const std::string& name) {
    if (name == "rgba8unorm") return TextureFormat::Rgba8Unorm;
    if (name == "rgba16float") return TextureFormat::Rgba16Float;
    if (name == "rgba32float") return TextureFormat::Rgba32Float;
    if (name == "r32float") return TextureFormat::R32Float;
    return std::nullopt;
}

/// `texture_storage_2d<rgba16float, write>` -> Art und Format.
Result<void> readStorageTexture(const std::string& type, DeclaredBinding& out) {
    const std::string compact = squeeze(type);
    constexpr std::string_view prefix = "texture_storage_2d<";
    if (compact.rfind(prefix, 0) != 0 || compact.back() != '>') {
        return bad("Nicht unterstützter Ressourcentyp: " + type);
    }
    const std::string inner = compact.substr(prefix.size(), compact.size() - prefix.size() - 1);
    const std::size_t comma = inner.find(',');
    if (comma == std::string::npos) {
        return bad("Storage-Textur ohne Zugriffsart: " + type);
    }
    const std::optional<TextureFormat> format = formatFromName(inner.substr(0, comma));
    if (!format) {
        return bad("Nicht unterstütztes Storage-Textur-Format: " + inner.substr(0, comma));
    }
    const std::string access = inner.substr(comma + 1);
    if (access == "write") {
        out.kind = BindingKind::StorageTextureWrite;
    } else if (access == "read") {
        out.kind = BindingKind::StorageTextureRead;
    } else if (access == "read_write") {
        out.kind = BindingKind::StorageTextureReadWrite;
    } else {
        return bad("Unbekannte Zugriffsart der Storage-Textur: " + access);
    }
    out.format = *format;
    return {};
}

}  // namespace

Result<std::vector<DeclaredBinding>> readBindings(std::string_view wgsl) {
    const std::string text = stripComments(wgsl);
    std::vector<DeclaredBinding> found;

    std::size_t at = 0;
    while ((at = text.find("@group", at)) != std::string::npos) {
        Cursor c{text, at + 6};
        DeclaredBinding decl;

        const bool head = c.consume('(');
        const std::optional<std::uint32_t> group = head ? c.number() : std::nullopt;
        if (!group || !c.consume(')')) {
            return bad("@group ohne gültige Nummer");
        }
        if (!c.consumeWord("@binding") || !c.consume('(')) {
            return bad("@group muss unmittelbar von @binding gefolgt sein");
        }
        const std::optional<std::uint32_t> binding = c.number();
        if (!binding || !c.consume(')')) {
            return bad("@binding ohne gültige Nummer");
        }
        decl.group = *group;
        decl.binding = *binding;

        if (!c.consumeWord("var")) {
            return bad("Nach @group/@binding wird eine var-Deklaration erwartet");
        }
        std::string space;
        if (c.consume('<')) {
            const std::optional<std::string> inside = c.until('>');
            if (!inside || !c.consume('>')) {
                return bad("var<...> ist nicht abgeschlossen");
            }
            space = squeeze(*inside);
        }
        decl.name = c.identifier();
        if (decl.name.empty() || !c.consume(':')) {
            return bad("Ressource ohne Name oder Typ");
        }
        const std::optional<std::string> type = c.until(';');
        if (!type) {
            return bad("Deklaration von " + decl.name + " endet nicht mit ';'");
        }

        if (space.empty()) {
            if (Result<void> ok = readStorageTexture(*type, decl); !ok.ok()) {
                return ok.error();
            }
        } else if (space == "uniform") {
            decl.kind = BindingKind::UniformBuffer;
        } else if (space == "storage" || space == "storage,read") {
            decl.kind = BindingKind::StorageBufferRead;
        } else if (space == "storage,read_write") {
            decl.kind = BindingKind::StorageBufferReadWrite;
        } else {
            return bad("Nicht unterstützter Adressraum: var<" + space + ">");
        }

        const auto duplicate = std::find_if(found.begin(), found.end(), [&](const DeclaredBinding& d) {
            return d.group == decl.group && d.binding == decl.binding;
        });
        if (duplicate != found.end()) {
            return bad("Bindung doppelt belegt: @group(" + std::to_string(decl.group) + ") @binding(" +
                       std::to_string(decl.binding) + ")");
        }
        found.push_back(std::move(decl));
        at = c.position();
    }

    std::sort(found.begin(), found.end(), [](const DeclaredBinding& a, const DeclaredBinding& b) {
        return a.group != b.group ? a.group < b.group : a.binding < b.binding;
    });
    return found;
}

}  // namespace whitehole::gpu
