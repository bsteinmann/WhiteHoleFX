// Text und Laufschrift: der Node wh.generator.text.
//
// Warum es diese Datei gibt: Schrift gehört zu den ersten Wünschen an eine Pixelwand. Der Node zeichnet
// den Text in einer 5x7-Punktschrift (builtin_text_font.hpp) direkt im Shader, ohne Textur und ohne
// Schriftdatei. Eine Schriftzelle ist 8 Punkte hoch (7 plus eine Zeile für Umlautpunkte) und `5 + spacing`
// Punkte breit; `height` bestimmt, wie viel der Leinwandhöhe die acht Zeilen füllen. Passt die Höhe zur Wand
// (bei 16 Zeilen: height 0.5 = ein Schriftpunkt je LED), werden die Punkte gleich groß wie die LEDs.
//
// Der Text kommt als Parameter (UTF-8) und wird beim Kompilieren in den Shader geschrieben (@text, @textlen);
// ein neuer Text baut die Pipeline neu, ein neues Tempo nur den Puffer. Der Shader weiß nicht, wie breit die
// Leinwand ist; `aspect` (Breite / Höhe, 32x16 = 2) gleicht das aus, damit die Punkte quadratisch bleiben.
// Rollen und Stillstand: mode Scroll schiebt den Text mit `speed` Schriftpunkten pro Sekunde durch (negativ:
// nach rechts) und beginnt nach `gap` Punkten Pause von vorn; Static setzt ihn links, mittig oder rechts.
// Die Position ist auf ganze Schriftpunkte gerundet, die Schrift bleibt scharf.
#include "builtin_internal.hpp"
#include "builtin_text_font.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kTextBody = R"wgsl(var txt = @text(text);
let n = i32(@textlen(text));
if (n == 0) {
    return @param(background);
}
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
if (u < 0.0 || rowF < 0.0 || rowF >= 8.0) {
    return @param(background);
}
let ci = i32(floor(u / advance));
if (ci >= n) {
    return @param(background);
}
let col = i32(floor(u - f32(ci) * advance));
if (col > 4) {
    return @param(background);
}
let glyph = wh_text_glyph(txt[ci]);
if (wh_text_bit(glyph, u32(col), u32(floor(rowF)))) {
    return @param(color);
}
return @param(background);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> textNodes() {
    std::vector<compiler::NodeDefinition> nodes;
    nodes.push_back(fieldNode(
        "wh.generator.text", "Text", "Generators", {fieldOutput()},
        {graph::ParameterDescriptor{"text", "Text", std::string{"WhiteHoleFX"}, std::nullopt, std::nullopt},
         choice("mode", "Mode", {"Scroll", "Static"}), number("speed", "Speed (dots/s)", 12.0, -200.0, 200.0),
         number("gap", "Pause before repeat (dots)", 8.0, 0.0, 200.0),
         choice("align", "Align (static)", {"Left", "Center", "Right"}, 1),
         number("height", "Height", 0.5, 0.05, 4.0), number("y", "Vertical center", 0.5, -1.0, 2.0),
         number("aspect", "Canvas aspect (w/h)", 2.0, 0.1, 20.0), integer("spacing", "Letter spacing (dots)", 1, 0, 8),
         color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         color("background", "Background", ColorRgba{0.0F, 0.0F, 0.0F, 0.0F})},
        kTextBody, kTextPrelude));
    return nodes;
}

}  // namespace whitehole::nodes::detail
