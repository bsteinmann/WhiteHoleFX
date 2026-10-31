// Formen und Muster: Metaballs, Sternenhimmel, Funkeln, wachsende Formen, Sinuslinien.
//
// Warum eine eigene Datei: builtin_generators.cpp enthält die Grundgeneratoren (Verlauf, Rauschen,
// Plasma ...) und bleibt damit kurz genug. Alle Nodes hier sind zustandslos: Das Bild hängt nur
// von Koordinate und Zeit ab. Zufall kommt aus einem Hash der Zelle, nicht aus einem Zustand.
//
// Regel für Preludes: Ein Node hat genau ein Prelude, und der Compiler legt nur *gleiche* Preludes
// zusammen. Ein Prelude mit eigenen Funktionen darf darum keine Funktionen eines anderen enthalten
// (zum Beispiel kColorPrelude); sonst wären sie in einem Graphen doppelt definiert. Die Nodes mit
// Hash mischen Farben deshalb im Rumpf selbst.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Hash einer Zelle (zwei Ganzzahlen als Float) -> 0..1, ohne Sinus (Dave Hoskins). Starfield und
// Fading Pixels teilen dieses Prelude.
constexpr const char* kCellHashPrelude = R"wgsl(fn wh_cell_hash(p: vec2<f32>) -> f32 {
    var q = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    q = q + dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
)wgsl";

// Metaballs: bis zu sechs weiche Kugeln wandern auf Lissajous-Bahnen und fließen ineinander, wenn sie
// sich nähern. Jede trägt r^2 / Abstand^2 zu einem Feld bei; wo die Summe 1 übersteigt, ist Blob.
// `softness` ist die Breite des Rands, gemessen am Feld (nicht in Koordinaten).
constexpr const char* kMetaballsBody = R"wgsl(let n = i32(clamp(floor(@param(count)), 1.0, 6.0));
let ph = @time * @param(speed);
let r = max(@param(size), 0.001);
var f = 0.0;
for (var i = 0; i < 6; i = i + 1) {
    if (i < n) {
        let fi = f32(i);
        let c = vec2<f32>(0.5 + 0.35 * sin(ph * (0.7 + 0.23 * fi) + fi * 2.1),
                          0.5 + 0.35 * cos(ph * (0.5 + 0.19 * fi) + fi * 1.3));
        let d = p.xy - c;
        f = f + r * r / (dot(d, d) + 0.0001);
    }
}
let s = clamp(@param(softness), 0.01, 2.0);
let v = smoothstep(1.0 - s * 0.5, 1.0 + s * 0.5, f);
return wh_mix_color(@param(colorA), @param(colorB), v);)wgsl";

// Sternenhimmel: ein Raster aus `cells` Zellen je Achse; in jeder Zelle sitzt mit der Wahrscheinlichkeit
// `density` ein Stern, der in eigenem Tempo flimmert. `drift` schiebt den Himmel (positiv nach unten).
// Der Hintergrund ist durchsichtig, der Himmel lässt sich also über andere Effekte legen.
constexpr const char* kStarfieldBody = R"wgsl(let cells = max(floor(@param(cells)), 1.0);
let q = vec2<f32>(p.x, p.y + @time * @param(drift)) * cells;
let id = floor(q);
let h1 = wh_cell_hash(id);
let h2 = wh_cell_hash(id + vec2<f32>(13.0, 7.0));
let h3 = wh_cell_hash(id + vec2<f32>(3.0, 41.0));
let on = select(0.0, 1.0, h1 < @param(density));
let tw = 0.65 + 0.35 * sin(@time * @param(twinkle) * (1.0 + 3.0 * h2) + h3 * 6.2831853);
let d = length(fract(q) - vec2<f32>(0.5, 0.5)) * 2.0;
let sz = clamp(@param(size), 0.05, 1.0);
let star = 1.0 - smoothstep(sz * 0.5, sz, d);
let col = mix(@param(colorA), @param(colorB), h2);
return vec4<f32>(col.rgb, col.a * star * tw * on);)wgsl";

// Fading Pixels: einzelne Zellen leuchten auf und verlöschen. Jede Zelle hat einen eigenen Takt; zu
// Beginn jedes Takts wird neu gewürfelt, ob sie leuchtet (`density`). `sharpness` ist der Exponent
// der Abklingkurve (größer = kurzer Blitz mit langem Dunkel).
constexpr const char* kFadingBody = R"wgsl(let cells = max(floor(@param(cells)), 1.0);
let id = floor(vec2<f32>(p.x, p.y) * cells);
let h1 = wh_cell_hash(id);
let h2 = wh_cell_hash(id + vec2<f32>(13.0, 7.0));
let h3 = wh_cell_hash(id + vec2<f32>(3.0, 41.0));
let t = @time * @param(rate) * mix(0.6, 1.4, h2) + h3;
let age = fract(t);
let cycle = floor(t);
let lit = wh_cell_hash(id + vec2<f32>(cycle * 7.13, cycle * 3.71)) < @param(density);
let fade = pow(1.0 - age, max(@param(sharpness), 0.1));
let col = mix(@param(colorA), @param(colorB), h1);
return vec4<f32>(col.rgb, col.a * fade * select(0.0, 1.0, lit));)wgsl";

// Wachsende Formen: Kreise, Quadrate oder Rauten, die von einem Punkt nach außen laufen. Der Abstand d
// ist je nach Form euklidisch, Maximum oder Summe der Beträge; die Ringe liegen im Abstand `spacing`
// und wandern mit `speed` (negativ: nach innen). `fade` lässt sie nach außen verblassen.
constexpr const char* kShapesBody = R"wgsl(let d0 = p.xy - vec2<f32>(@param(cx), @param(cy));
let shape = i32(floor(@param(shape) + 0.5));
var d = length(d0);
if (shape == 1) {
    d = max(abs(d0.x), abs(d0.y));
}
if (shape == 2) {
    d = abs(d0.x) + abs(d0.y);
}
let spacing = max(@param(spacing), 0.02);
let ph = fract((d - @time * @param(speed)) / spacing);
let w = clamp(@param(thickness), 0.02, 1.0);
let ring = 1.0 - smoothstep(0.0, w * 0.5, abs(ph - 0.5));
let v = ring * clamp(1.0 - d * @param(fade), 0.0, 1.0);
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

// Sinuslinien: bis zu acht Wellen quer über die Leinwand, jede gegen die vorige um `spread` Perioden
// verschoben. Die Farbe läuft von Linie zu Linie von colorA nach colorB. Überlappende Linien addieren
// sich (vormultipliziert, wie wh_add_color).
constexpr const char* kSineLinesBody = R"wgsl(let n = clamp(floor(@param(count)), 1.0, 8.0);
var light = vec3<f32>(0.0, 0.0, 0.0);
var asum = 0.0;
for (var i = 0; i < 8; i = i + 1) {
    let fi = f32(i);
    if (fi < n) {
        let y = 0.5 + @param(amplitude) * sin(6.2831853 * (@param(frequency) * p.x + fi * @param(spread)) + @time * @param(speed));
        let v = 1.0 - smoothstep(0.0, max(@param(thickness), 0.002), abs(p.y - y));
        let col = mix(@param(colorA), @param(colorB), select(0.0, fi / max(n - 1.0, 1.0), n > 1.0));
        light = light + col.rgb * col.a * v;
        asum = asum + col.a * v;
    }
}
let alpha = clamp(asum, 0.0, 1.0);
return vec4<f32>(min(light / max(alpha, 0.000001), vec3<f32>(1.0, 1.0, 1.0)), alpha);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> shapeNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode(
        "wh.generator.metaballs", "Metaballs", "Generators", {fieldOutput()},
        {integer("count", "Balls", 3, 1, 6), number("size", "Size", 0.15, 0.01, 1.0),
         number("speed", "Speed", 0.6, -10.0, 10.0), number("softness", "Edge softness", 0.3, 0.01, 2.0),
         color("colorA", "Background", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Blobs", ColorRgba{1.0F, 0.2F, 0.6F, 1.0F})},
        kMetaballsBody, kColorPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.starfield", "Starfield", "Generators", {fieldOutput()},
        {number("cells", "Cells per axis", 16.0, 1.0, 256.0), number("density", "Star density", 0.15, 0.0, 1.0),
         number("size", "Star size", 0.6, 0.05, 1.0), number("twinkle", "Twinkle speed", 1.5, 0.0, 20.0),
         number("drift", "Drift (down)", 0.0, -5.0, 5.0),
         color("colorA", "Color A", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.6F, 0.75F, 1.0F, 1.0F})},
        kStarfieldBody, kCellHashPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.fading", "Fading Pixels", "Generators", {fieldOutput()},
        {number("cells", "Cells per axis", 16.0, 1.0, 256.0), number("rate", "Rate", 0.6, 0.01, 20.0),
         number("density", "Density", 0.4, 0.0, 1.0), number("sharpness", "Sharpness", 2.0, 0.1, 10.0),
         color("colorA", "Color A", ColorRgba{1.0F, 0.4F, 0.1F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.2F, 0.5F, 1.0F, 1.0F})},
        kFadingBody, kCellHashPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.shapes", "Expanding Shapes", "Generators", {fieldOutput()},
        {choice("shape", "Shape", {"Circle", "Square", "Diamond"}), number("cx", "Center X", 0.5, 0.0, 1.0),
         number("cy", "Center Y", 0.5, 0.0, 1.0), number("spacing", "Spacing", 0.3, 0.02, 2.0),
         number("speed", "Speed", 0.2, -5.0, 5.0), number("thickness", "Thickness", 0.3, 0.02, 1.0),
         number("fade", "Fade out", 0.5, 0.0, 4.0), color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kShapesBody));

    nodes.push_back(fieldNode(
        "wh.generator.sinelines", "Sine Lines", "Generators", {fieldOutput()},
        {integer("count", "Lines", 3, 1, 8), number("amplitude", "Amplitude", 0.25, 0.0, 0.5),
         number("frequency", "Frequency", 1.5, 0.0, 16.0), number("speed", "Speed", 1.0, -10.0, 10.0),
         number("spread", "Phase spread", 0.15, -2.0, 2.0), number("thickness", "Thickness", 0.06, 0.002, 0.5),
         color("colorA", "First line", ColorRgba{1.0F, 0.3F, 0.1F, 1.0F}),
         color("colorB", "Last line", ColorRgba{0.2F, 0.6F, 1.0F, 1.0F})},
        kSineLinesBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
