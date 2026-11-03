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

// Treffer-Blitz: füllt das Bild (oder eine Form) mit `level`, gedacht für den Puls eines Kick-Erkenners.
// Der Rest bleibt durchsichtig, der Blitz liegt also dunkel auf dunkel und lässt sich über andere Looks legen.
// Form 0 füllt alles, 1 ist eine Scheibe aus der Mitte, 2 ein Rahmen am Rand, 3 ein senkrechter Balken
// in der Mitte; `size` ist ihre Größe (0..1), `softness` die Breite des Rands.
constexpr const char* kHitFlashBody = R"wgsl(let q = p.xy - vec2<f32>(0.5, 0.5);
let shape = i32(floor(@param(shape) + 0.5));
let s = clamp(@param(size), 0.0, 1.0);
let soft = max(@param(softness), 0.001);
var m = 1.0;
if (shape == 1) {
    let r = s * 0.75;
    m = 1.0 - smoothstep(r - soft, r, length(q));
}
if (shape == 2) {
    let e = 0.5 - max(abs(q.x), abs(q.y));
    let w = s * 0.5;
    m = 1.0 - smoothstep(w - soft, w, e);
}
if (shape == 3) {
    let w = s * 0.5;
    m = 1.0 - smoothstep(w - soft, w, abs(q.x));
}
let col = @param(color);
return vec4<f32>(col.rgb, col.a * clamp(@param(level), 0.0, 1.0) * m);)wgsl";

// Schockwelle: ein Ring (Kreis, Quadrat oder Raute), der `age` Sekunden nach dem Treffer mit `speed` nach
// außen gelaufen ist und über `life` Sekunden verblasst. `age` kommt vom Ausgang "Age" des Kick-Erkenners;
// der Standard 100 heißt: schon vergangen, kein Ring. Mit space = Volume wird daraus eine Kugelschale.
constexpr const char* kShockwaveBody = R"wgsl(let center = vec3<f32>(@param(cx), @param(cy), @param(cz));
let d0 = select(vec3<f32>(p.xy - center.xy, 0.0), p - center, ((@param(space) > 0.5 && @param(space) < 1.5) || (@param(space) >= 1.5 && @volume)));
let shape = i32(floor(@param(shape) + 0.5));
var d = length(d0);
if (shape == 1) {
    d = max(max(abs(d0.x), abs(d0.y)), abs(d0.z));
}
if (shape == 2) {
    d = abs(d0.x) + abs(d0.y) + abs(d0.z);
}
let age = max(@param(age), 0.0);
let life = max(@param(life), 0.05);
let w = max(@param(thickness), 0.005);
let ring = 1.0 - smoothstep(0.0, w, abs(d - age * @param(speed)));
let fade = clamp(1.0 - age / life, 0.0, 1.0);
let col = @param(color);
return vec4<f32>(col.rgb, col.a * ring * fade);)wgsl";

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
    nodes.push_back(fieldNode(
        "wh.generator.hitflash", "Hit Flash", "Audio", {fieldOutput()},
        {number("level", "Level", 0.0, 0.0, 1.0), choice("shape", "Shape", {"Full", "Disc", "Frame", "Bar"}),
         number("size", "Size", 0.6, 0.0, 1.0), number("softness", "Edge softness", 0.1, 0.0, 0.5),
         color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kHitFlashBody));
    nodes.push_back(fieldNode(
        "wh.generator.shockwave", "Shockwave", "Audio", {fieldOutput()},
        {number("age", "Age since hit (s)", 100.0, 0.0, 100.0), number("speed", "Speed", 1.2, 0.0, 10.0),
         number("life", "Lifetime (s)", 0.8, 0.05, 10.0), number("thickness", "Ring width", 0.08, 0.005, 0.5),
         choice("shape", "Shape", {"Circle", "Square", "Diamond"}), number("cx", "Center X", 0.5, 0.0, 1.0),
         number("cy", "Center Y", 0.5, 0.0, 1.0), number("cz", "Center Z", 0.5, 0.0, 1.0),
         color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         choice("space", "Space", {"Flat (x/y)", "Volume (x/y/z)", "Auto (by layout)"}, 2)},
        kShockwaveBody));
    return nodes;
}

}  // namespace whitehole::nodes::detail
