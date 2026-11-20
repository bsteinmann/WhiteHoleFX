// Formen und Muster: Metaballs, Sternenhimmel, Funkeln, Zufallspixel, Sinuslinien (Expanding Shapes: builtin_expanding.cpp).
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
// `softness` ist die Breite des Rands, gemessen am Feld (nicht in Koordinaten). Mit space = Volume bekommt
// jede Kugel auch eine z-Bahn und der Abstand wird im Raum gemessen; Flat rechnet wie bisher nur in x/y.
constexpr const char* kMetaballsBody = R"wgsl(let n = i32(clamp(floor(@param(count)), 1.0, 6.0));
let ph = @time * @param(speed);
let r = max(@param(size), 0.001);
var f = 0.0;
for (var i = 0; i < 6; i = i + 1) {
    if (i < n) {
        let fi = f32(i);
        let c = vec2<f32>(0.5 + 0.35 * sin(ph * (0.7 + 0.23 * fi) + fi * 2.1),
                          0.5 + 0.35 * cos(ph * (0.5 + 0.19 * fi) + fi * 1.3));
        let cz = 0.5 + 0.35 * sin(ph * (0.6 + 0.17 * fi) + fi * 0.9);
        let d = select(vec3<f32>(p.xy - c, 0.0), p - vec3<f32>(c, cz), ((@param(space) > 0.5 && @param(space) < 1.5) || (@param(space) >= 1.5 && @volume)));
        f = f + r * r / (dot(d, d) + 0.0001);
    }
}
let s = clamp(@param(softness), 0.01, 2.0);
let v = smoothstep(1.0 - s * 0.5, 1.0 + s * 0.5, f);
return wh_mix_color(@param(colorA), @param(colorB), v);)wgsl";

// Sternenhimmel: ein Raster aus `cells` Zellen je Achse; in jeder Zelle sitzt mit der Wahrscheinlichkeit
// `density` ein Stern, der in eigenem Tempo flimmert. `drift` schiebt den Himmel (positiv nach unten).
// Der Hintergrund ist durchsichtig, der Himmel lässt sich also über andere Effekte legen.
// Mit space = Volume sitzen die Sterne in einem Raster aus Würfeln statt Quadraten (die Zelle bekommt ihren
// Zufall aus x, y und z); Flat ignoriert z und ist damit gleich dem 2D-Himmel von früher.
//
// `motion` = Warp: statt zu flimmern, laufen die Sterne von der Mitte (0.5, 0.5) radial nach außen und
// werden dabei zu Streifen (Sprung auf Warp-/Hyperraum-Geschwindigkeit). `cells` teilt den Kreis in
// gleich viele Winkel-"Spuren" (eine Spur = ein Stern, der endlos nach außen läuft und am Rand wieder bei
// der Mitte neu beginnt); `warpSpeed` ist das Tempo, `trail` die Streifenlänge relativ zum aktuellen
// Abstand von der Mitte (also kurz nahe der Mitte, lang am Rand, wie bei echtem Warp). Ignoriert dabei
// `space`/`drift`/`twinkle`/`size` (reiner 2D-Effekt in x/y, unabhängig von Flat/Volume).
constexpr const char* kStarfieldBody = R"wgsl(let autoCells = @param(cells) < 0.5;
let colsAuto = max(select(16.0, @columns, @columns > 0.0), 1.0);
let rowsAuto = max(select(16.0, @rows, @rows > 0.0), 1.0);
let cells = select(max(floor(@param(cells)), 1.0), colsAuto, autoCells);
let cellScale = select(vec3<f32>(cells, cells, cells), vec3<f32>(colsAuto, rowsAuto, colsAuto), autoCells);
if (@param(motion) > 0.5) {
    let centered = p.xy - vec2<f32>(0.5, 0.5);
    let r = length(centered);
    let theta = atan2(centered.y, centered.x);
    let lane = floor((theta + 3.14159265) / 6.2831853 * cells);
    let hp = wh_cell_hash(vec2<f32>(lane, 0.0));
    let hc = wh_cell_hash(vec2<f32>(lane, 1.0));
    let hd = wh_cell_hash(vec2<f32>(lane, 2.0));
    let maxR = 0.75;  // reicht über die Bildecke hinaus (Abstand Mitte -> Ecke ist rund 0.707)
    let headR = fract(hp + @time * @param(warpSpeed) * (0.5 + hc)) * maxR;
    let trailLen = max(clamp(@param(trail), 0.0, 1.0) * headR, 0.001);
    let behind = headR - r;
    let fade = 1.0 - clamp(behind / trailLen, 0.0, 1.0);
    let on = select(0.0, 1.0, behind >= 0.0 && behind <= trailLen && hd < @param(density));
    let col = mix(@param(colorA), @param(colorB), hc);
    return vec4<f32>(col.rgb, col.a * fade * on);
}
let vol = ((@param(space) > 0.5 && @param(space) < 1.5) || (@param(space) >= 1.5 && @volume));
let q = vec3<f32>(p.x, p.y + @time * @param(drift), select(0.0, p.z, vol)) * cellScale;
let cell3 = floor(q);
let id = cell3.xy + vec2<f32>(cell3.z * 19.0, cell3.z * 11.0);
let h1 = wh_cell_hash(id);
let h2 = wh_cell_hash(id + vec2<f32>(13.0, 7.0));
let h3 = wh_cell_hash(id + vec2<f32>(3.0, 41.0));
let on = select(0.0, 1.0, h1 < @param(density));
let tw = 0.65 + 0.35 * sin(@time * @param(twinkle) * (1.0 + 3.0 * h2) + h3 * 6.2831853);
let f = fract(q) - vec3<f32>(0.5, 0.5, 0.5);
let d = length(vec3<f32>(f.x, f.y, select(0.0, f.z, vol))) * 2.0;
let sz = clamp(@param(size), 0.05, 1.0);
let star = 1.0 - smoothstep(sz * 0.5, sz, d);
let col = mix(@param(colorA), @param(colorB), h2);
return vec4<f32>(col.rgb, col.a * star * tw * on);)wgsl";

// Fading Pixels: einzelne Zellen leuchten auf und verlöschen. Jede Zelle hat einen eigenen Takt; zu
// Beginn jedes Takts wird neu gewürfelt, ob sie leuchtet (`density`). `sharpness` ist der Exponent
// der Abklingkurve (größer = kurzer Blitz mit langem Dunkel). Mit space = Volume ist jede Zelle ein Würfel
// des Raums (eigener Takt je Würfel), Flat gruppiert nur nach x/y wie bisher.
constexpr const char* kFadingBody = R"wgsl(let autoCells = @param(cells) < 0.5;
let colsAuto = max(select(16.0, @columns, @columns > 0.0), 1.0);
let rowsAuto = max(select(16.0, @rows, @rows > 0.0), 1.0);
let cells = max(floor(@param(cells)), 1.0);
let cellScale = select(vec3<f32>(cells, cells, cells), vec3<f32>(colsAuto, rowsAuto, colsAuto), autoCells);
let cell3 = floor(vec3<f32>(p.x, p.y, select(0.0, p.z, ((@param(space) > 0.5 && @param(space) < 1.5) || (@param(space) >= 1.5 && @volume)))) * cellScale);
let id = cell3.xy + vec2<f32>(cell3.z * 19.0, cell3.z * 11.0);
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

// Zufallspixel (Issue #24): Das Raster hat `columns` x `rows` Zellen und passt damit zu einer Wand (Standard: 0 =
// automatisch ein Feld je LED aus der echten Auflösung, `@columns`/`@rows`; ein Wert > 0 bleibt als manueller
// Override erhalten, ohne bekannte Auflösung Rückfall auf 32 x 16). Jedes Pixel hat einen eigenen Takt von `rate`
// Schritten pro Sekunde; in jedem Schritt leuchtet es mit der Wahrscheinlichkeit `density` auf, in der Farbe
// zwischen A und B nach Zufall. Blink schaltet hart an und aus, Fade lässt es über den Schritt abklingen. Der
// Rest ist durchsichtig. Schritte werden mit 997 abgeschnitten, damit der Hash nach Tagen Laufzeit nicht an
// Float-Genauigkeit verliert.
constexpr const char* kRandomPixelsBody = R"wgsl(let colsAuto = select(32.0, @columns, @columns > 0.0);
let rowsAuto = select(16.0, @rows, @rows > 0.0);
let cols = select(colsAuto, max(floor(@param(columns)), 1.0), @param(columns) > 0.0);
let rows = select(rowsAuto, max(floor(@param(rows)), 1.0), @param(rows) > 0.0);
let cell = min(floor(vec2<f32>(p.x * cols, p.y * rows)), vec2<f32>(cols - 1.0, rows - 1.0));
let phase = wh_cell_hash(cell + vec2<f32>(13.0, 7.0));
let t = @time * max(@param(rate), 0.0) + phase;
let whole = floor(t);
let step = whole - 997.0 * floor(whole / 997.0);
let lit = wh_cell_hash(cell + vec2<f32>(step * 7.13, step * 3.71)) < @param(density);
let age = fract(t);
let level = select(1.0, 1.0 - age, @param(mode) > 0.5);
let pick = wh_cell_hash(cell + vec2<f32>(step * 5.31 + 3.0, step * 2.17 + 41.0));
let col = mix(@param(colorA), @param(colorB), pick);
return vec4<f32>(col.rgb, col.a * level * select(0.0, 1.0, lit));)wgsl";

// Sinuslinien: bis zu acht Wellen quer über die Leinwand, jede gegen die vorige um `spread` Grad
// phasenverschoben (intuitiv in Grad statt Perioden, wie die Rotation bei Expanding Shapes). Jede Linie
// hat ihre eigene feste Farbe (`color1`..`color8`, nicht mehr von/bis interpoliert). Überlappende Linien
// addieren sich (vormultipliziert, wie wh_add_color). `frequencyZ` schiebt die Phase mit z: Aus jeder
// Linie wird im Würfel eine gewellte Fläche (0 = in z überall gleich, wie auf der 2D-Leinwand).
constexpr const char* kSineLinesBody = R"wgsl(let n = clamp(floor(@param(count)), 1.0, 8.0);
let colors = array<vec4<f32>, 8>(@param(color1), @param(color2), @param(color3), @param(color4), @param(color5), @param(color6), @param(color7), @param(color8));
let spreadRad = radians(@param(spread));
var light = vec3<f32>(0.0, 0.0, 0.0);
var asum = 0.0;
for (var i = 0; i < 8; i = i + 1) {
    let fi = f32(i);
    if (fi < n) {
        let y = 0.5 + @param(amplitude) * sin(6.2831853 * (@param(frequency) * p.x + @param(frequencyZ) * p.z) + @time * @param(speed) + fi * spreadRad);
        let v = 1.0 - smoothstep(0.0, max(@param(thickness), 0.002), abs(p.y - y));
        let col = colors[i];
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
         color("colorB", "Blobs", ColorRgba{1.0F, 0.2F, 0.6F, 1.0F}),
         choice("space", "Space", {"Flat (x/y)", "Volume (x/y/z)", "Auto (by layout)"}, 2)},
        kMetaballsBody, kColorPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.starfield", "Starfield", "Generators", {fieldOutput()},
        {number("cells", "Cells per axis (0 = automatic)", 0.0, 0.0, 256.0), number("density", "Star density", 0.15, 0.0, 1.0),
         number("size", "Star size", 0.6, 0.05, 1.0), number("twinkle", "Twinkle speed", 1.5, 0.0, 20.0),
         number("drift", "Drift (down)", 0.0, -5.0, 5.0),
         color("colorA", "Color A", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.6F, 0.75F, 1.0F, 1.0F}),
         choice("space", "Space", {"Flat (x/y)", "Volume (x/y/z)", "Auto (by layout)"}, 2),
         choice("motion", "Motion", {"Twinkle", "Warp (streaks from center)"}),
         number("warpSpeed", "Warp speed", 0.5, 0.0, 5.0), number("trail", "Warp trail length", 0.5, 0.0, 1.0)},
        kStarfieldBody, kCellHashPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.fading", "Fading Pixels", "Generators", {fieldOutput()},
        {number("cells", "Cells per axis (0 = automatic)", 0.0, 0.0, 256.0), number("rate", "Rate", 0.6, 0.01, 20.0),
         number("density", "Density", 0.4, 0.0, 1.0), number("sharpness", "Sharpness", 2.0, 0.1, 10.0),
         color("colorA", "Color A", ColorRgba{1.0F, 0.4F, 0.1F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.2F, 0.5F, 1.0F, 1.0F}),
         choice("space", "Space", {"Flat (x/y)", "Volume (x/y/z)", "Auto (by layout)"}, 2)},
        kFadingBody, kCellHashPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.randompixels", "Random Pixels", "Generators", {fieldOutput()},
        {integer("columns", "Columns (LEDs across, 0 = automatic)", 0, 0, 512),
         integer("rows", "Rows (LEDs high, 0 = automatic)", 0, 0, 512),
         number("density", "Density (share lit)", 0.15, 0.0, 1.0), number("rate", "Steps per second", 4.0, 0.0, 60.0),
         choice("mode", "Mode", {"Blink", "Fade"}),
         color("colorA", "Color A", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.3F, 0.7F, 1.0F, 1.0F})},
        kRandomPixelsBody, kCellHashPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.sinelines", "Sine Lines", "Generators", {fieldOutput()},
        {integer("count", "Lines", 3, 1, 8), number("amplitude", "Amplitude", 0.25, 0.0, 0.5),
         number("frequency", "Frequency", 1.5, 0.0, 16.0), number("speed", "Speed", 1.0, -10.0, 10.0),
         number("spread", "Phase spread (deg)", 54.0, -720.0, 720.0), number("thickness", "Thickness", 0.06, 0.002, 0.5),
         color("color1", "Line 1 color", ColorRgba{1.00F, 0.30F, 0.10F, 1.0F}),
         color("color2", "Line 2 color", ColorRgba{0.89F, 0.34F, 0.23F, 1.0F}),
         color("color3", "Line 3 color", ColorRgba{0.77F, 0.39F, 0.36F, 1.0F}),
         color("color4", "Line 4 color", ColorRgba{0.66F, 0.43F, 0.49F, 1.0F}),
         color("color5", "Line 5 color", ColorRgba{0.54F, 0.47F, 0.61F, 1.0F}),
         color("color6", "Line 6 color", ColorRgba{0.43F, 0.51F, 0.74F, 1.0F}),
         color("color7", "Line 7 color", ColorRgba{0.31F, 0.56F, 0.87F, 1.0F}),
         color("color8", "Line 8 color", ColorRgba{0.20F, 0.60F, 1.00F, 1.0F}),
         number("frequencyZ", "Frequency along z", 0.0, -16.0, 16.0)},
        kSineLinesBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
