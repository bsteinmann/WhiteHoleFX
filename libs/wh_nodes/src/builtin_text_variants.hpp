// builtin_text_variants.hpp: Shader-Varianten des Text-Nodes für die Fehlersuche (Issue #13).
//
// Warum es diese Datei gibt: Auf einer GeForce GTX 760 (Vulkan) bleibt die Schrift unsichtbar, während derselbe Shader
// auf Software-Vulkan und OpenGL (llvmpipe) funktioniert. Welche Stelle des Shaders der Treiber falsch übersetzt, lässt
// sich nur an der echten Karte klären. Mit der Umgebungsvariable WHITEHOLEFX_TEXT_VARIANT (Buchstaben a, b, c, auch
// kombiniert) schaltet man Umschreibungen einzeln ein, ohne neu zu bauen:
//   a  statt `switch` eine Kette aus `if` für die Umlaute
//   b  nur ein `return` am Ende (kein vorzeitiges Verlassen der Funktion)
//   c  die Schrifttabelle als lokale Variable in wh_text_bit statt als private Variable mit Anfangswert
// Messvarianten, die einzelne Stufen des Shaders ausblenden und so zeigen, welche Stufe auf der Karte versagt:
//   s  Startposition fest 0 (ohne Zeit und Laufschrift)
//   g  jedes Zeichen ist ein "A" (ohne den Text-Puffer txt[ci])
//   h  jeder Punkt der Schrift ist gesetzt (ohne Schrifttabelle und Bit-Auswahl)
//   k  das ganze Bild in der Textfarbe (prüft den Parameter `color`)
//   q  das ganze Bild in festem Rot (prüft, ob der Node überhaupt mitzeichnet)
// Ohne die Variable bleibt alles wie bisher. Wird die Ursache gefunden, wird die passende Form zur festen und die
// Umschaltung entfällt wieder.
#pragma once

#include <cstdlib>
#include <string>

namespace whitehole::nodes::detail {

inline bool textVariantOn(char letter) {
    const char* wanted = std::getenv("WHITEHOLEFX_TEXT_VARIANT");
    return wanted != nullptr && std::string{wanted}.find(letter) != std::string::npos;
}

inline void replaceOnce(std::string& text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    if (at != std::string::npos) {
        text.replace(at, from.size(), to);
    }
}

/// Prelude (Schrifttabelle und Hilfsfunktionen) in der gewählten Variante.
inline std::string textPreludeVariant(std::string prelude) {
    if (textVariantOn('a')) {
        const std::size_t start = prelude.find("    switch (cp) {");
        const std::size_t end = prelude.find("fn wh_text_bit");
        if (start != std::string::npos && end != std::string::npos) {
            prelude.replace(start, end - start,
                            "    if (cp == 0xE4u) { return 95u; }\n    if (cp == 0xF6u) { return 96u; }\n"
                            "    if (cp == 0xFCu) { return 97u; }\n    if (cp == 0xC4u) { return 98u; }\n"
                            "    if (cp == 0xD6u) { return 99u; }\n    if (cp == 0xDCu) { return 100u; }\n"
                            "    if (cp == 0xDFu) { return 101u; }\n    return 31u;\n}\n\n");
        }
    }
    if (textVariantOn('c')) {
        const std::string head = "var<private> wh_font: array<u32, 204> = ";
        const std::size_t start = prelude.find(head);
        const std::size_t end = prelude.find(");\n\nfn wh_text_glyph");
        if (start != std::string::npos && end != std::string::npos) {
            const std::string table = prelude.substr(start + head.size(), end + 1 - (start + head.size()));
            prelude.erase(start, end + 3 - start);
            replaceOnce(prelude, "    var bits = wh_font[glyph * 2u + 1u];",
                        "    var wh_font = " + table + ";\n    var bits = wh_font[glyph * 2u + 1u];");
        }
    }
    return prelude;
}

/// Körper mit nur einem `return` am Ende (Variante b).
inline std::string textBodySingleReturn() {
    return std::string{R"wgsl(var txt = @text(text);
let n = i32(@textlen(text));
var result = @param(background);
let fy = clamp(@param(height), 0.05, 4.0) / 8.0;
let fx = fy / max(@param(aspect), 0.1);
let canvasW = 1.0 / fx;
let spacing = max(floor(@param(spacing) + 0.5), 0.0);
let advance = 5.0 + spacing;
let textW = f32(n) * advance - spacing;
var start = 0.0;
if (i32(floor(@param(mode) + 0.5)) == 0) {
    let speed = @param(speed);
    let period = textW + canvasW + max(@param(gap), 0.0);
    let s = abs(speed) * @time;
    let m = s - period * floor(s / period);
    start = select(canvasW - m, m - textW, speed < 0.0);
} else {
    let a = i32(floor(@param(align) + 0.5));
    start = select(select(0.0, (canvasW - textW) * 0.5, a == 1), canvasW - textW, a == 2);
}
start = floor(start + 0.5);
let u = p.x / fx - start;
let rowF = (@param(y) + 0.5 * clamp(@param(height), 0.05, 4.0) - p.y) / fy;
let ci = i32(floor(u / advance));
let col = i32(floor(u - f32(ci) * advance));
if (n > 0 && u >= 0.0 && rowF >= 0.0 && rowF < 8.0 && ci < n && col <= 4) {
    let glyph = wh_text_glyph(txt[ci]);
    if (wh_text_bit(glyph, u32(col), u32(floor(rowF)))) {
        result = @param(color);
    }
}
return result;)wgsl"};
}

/// Rumpf des Nodes in der gewählten Variante.
inline std::string textBodyVariant(std::string body) {
    if (textVariantOn('b')) {
        body = textBodySingleReturn();
    }
    if (textVariantOn('s')) {
        replaceOnce(body, "start = floor(start + 0.5);", "start = 0.0;");
    }
    if (textVariantOn('g')) {
        replaceOnce(body, "wh_text_glyph(txt[ci])", "33u");
    }
    if (textVariantOn('h')) {
        replaceOnce(body, "if (wh_text_bit(glyph, u32(col), u32(floor(rowF)))) {", "if (true) {");
    }
    if (textVariantOn('k')) {
        body = "return @param(color);";
    }
    if (textVariantOn('q')) {
        body = "return vec4<f32>(1.0, 0.0, 0.0, 1.0);";
    }
    return body;
}


}  // namespace whitehole::nodes::detail
