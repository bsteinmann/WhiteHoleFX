// Audio-Anzeigen als Field-Nodes: Spectrum Bars (Equalizer) und Oscilloscope (Wellenform).
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

// Oszilloskop: die 32 Punkte der Wellenform (Porttyp AudioWave, -1..1) sind die Kurve über die Breite;
// zwischen den Punkten wird gerade verbunden. Der Abstand zu den drei Strecken um die Spalte gibt die
// Linie (auch bei steilen Flanken ohne Lücken). `amplitude` ist die halbe Höhe der Kurve, `fill` färbt
// die Fläche zwischen Mittellinie und Kurve. Ohne Verbindung ist die Kurve flach (Mittellinie).
constexpr const char* kScopePrelude = R"wgsl(fn wh_scope_dist(p: vec2<f32>, a: vec2<f32>, b: vec2<f32>) -> f32 {
    let ab = b - a;
    let t = clamp(dot(p - a, ab) / max(dot(ab, ab), 0.0000001), 0.0, 1.0);
    return length(p - (a + ab * t));
}
)wgsl";

constexpr const char* kScopeBody = R"wgsl(let u = clamp(p.x, 0.0, 1.0) * 31.0;
let i = min(u32(floor(u)), 30u);
let amp = @param(amplitude);
var dmin = 1000000.0;
for (var s = 0; s < 3; s = s + 1) {
    let a = i32(i) + s - 1;
    if (a >= 0 && a <= 30) {
        let ua = u32(a);
        let pa = vec2<f32>(f32(a) / 31.0, 0.5 + amp * @spectrum(wave, ua));
        let pb = vec2<f32>(f32(a + 1) / 31.0, 0.5 + amp * @spectrum(wave, ua + 1u));
        dmin = min(dmin, wh_scope_dist(p.xy, pa, pb));
    }
}
let w = max(@param(thickness), 0.002);
var v = clamp(1.0 - smoothstep(0.0, w, dmin) + @param(glow) * exp(-dmin / (w * 4.0)), 0.0, 1.0);
let yline = 0.5 + amp * mix(@spectrum(wave, i), @spectrum(wave, i + 1u), fract(u));
let dy = p.y - 0.5;
let inside = select(0.0, 1.0, dy * (yline - 0.5) > 0.0 && abs(dy) <= abs(yline - 0.5));
v = max(v, inside * clamp(@param(fill), 0.0, 1.0));
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

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
    nodes.push_back(fieldNode(
        "wh.generator.scope", "Oscilloscope", "Audio",
        {fieldOutput(),
         graph::PortDescriptor{"wave", "Wave", PortType::AudioWave, graph::PortDirection::Input, false}},
        {number("amplitude", "Amplitude", 0.4, 0.0, 0.5), number("thickness", "Line thickness", 0.05, 0.002, 0.3),
         number("glow", "Glow", 0.3, 0.0, 1.0), number("fill", "Fill", 0.0, 0.0, 1.0),
         color("color", "Color", ColorRgba{0.2F, 1.0F, 0.4F, 1.0F})},
        kScopeBody, kScopePrelude));
    return nodes;
}

}  // namespace whitehole::nodes::detail
