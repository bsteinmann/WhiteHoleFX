// Spektrum-Anzeige als Field-Node: ein Equalizer aus Balken über die ganze Leinwand.
//
// Warum es diese Datei gibt: Das Spektrum kommt als 32 Zahlen aus dem Node "Audio Spectrum" (CPU)
// und liegt im Uniform-Puffer des Feldshaders (Platzhalter @spectrum). Dieser Node zeichnet daraus
// Balken: Er teilt x in `bars` Spalten, nimmt je Spalte den größten Wert der zugehörigen Bänder und
// füllt von unten (oder von der Mitte aus) bis zu dieser Höhe. Ohne Verbindung sieht er Stille und
// bleibt durchsichtig, über anderen Effekten stört er also nicht.
//
// Konvention wie bei den anderen Generatoren: p.y = 0 ist unten.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kSpectrumBody = R"wgsl(let n = max(@param(bars), 1.0);
let x = clamp(p.x, 0.0, 0.99999);
let pos = x * n;
let idx = floor(pos);
let cell = fract(pos);
let halfGap = clamp(@param(gap), 0.0, 0.95) * 0.5;
if (cell < halfGap || cell > 1.0 - halfGap) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
}
let first = u32(floor(idx * 32.0 / n));
let last = max(u32(floor((idx + 1.0) * 32.0 / n)), first + 1u);
var level = 0.0;
for (var i = first; i < last; i = i + 1u) {
    level = max(level, @spectrum(spectrum, i));
}
var h = p.y;
if (@param(mode) > 0.5) {
    h = abs(p.y - 0.5) * 2.0;
}
let soft = max(@param(softness), 0.0001);
let cover = (1.0 - smoothstep(level - soft, level + soft, h)) * step(0.001, level);
let col = mix(@param(color), @param(colorTop), clamp(h, 0.0, 1.0));
return vec4<f32>(col.rgb, col.a * cover);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> audioFieldNodes() {
    using graph::PortType;
    std::vector<compiler::NodeDefinition> nodes;
    nodes.push_back(fieldNode(
        "wh.generator.spectrum", "Spectrum Bars", "Audio",
        {fieldOutput(),
         graph::PortDescriptor{"spectrum", "Spectrum", PortType::AudioSpectrum, graph::PortDirection::Input, false}},
        {integer("bars", "Bars", 16, 4.0, 32.0), number("gap", "Gap", 0.25, 0.0, 0.9),
         choice("mode", "Direction", {"From bottom", "From center"}), number("softness", "Edge softness", 0.03, 0.0, 0.5),
         color("color", "Bottom color", ColorRgba{0.1F, 1.0F, 0.3F, 1.0F}),
         color("colorTop", "Top color", ColorRgba{1.0F, 0.1F, 0.2F, 1.0F})},
        kSpectrumBody));
    return nodes;
}

}  // namespace whitehole::nodes::detail
