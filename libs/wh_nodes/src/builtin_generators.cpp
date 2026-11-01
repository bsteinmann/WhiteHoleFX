// Generatoren: Nodes ohne Field-Eingang, sie erzeugen Farbe aus Koordinate und Zeit.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Hash ohne Sinus ("hash without sine", Dave Hoskins) für 3D -> 1D, plus
// trilineare Value-Noise darüber. Die Zeit läuft in z-Richtung durch das Rauschen.
constexpr const char* kNoisePrelude = R"wgsl(fn wh_hash31(p: vec3<f32>) -> f32 {
    var q = fract(p * 0.1031);
    q = q + dot(q, q.zyx + 31.32);
    return fract((q.x + q.y) * q.z);
}

fn wh_value_noise(p: vec3<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    let u = f * f * (3.0 - 2.0 * f);
    let n000 = wh_hash31(i + vec3<f32>(0.0, 0.0, 0.0));
    let n100 = wh_hash31(i + vec3<f32>(1.0, 0.0, 0.0));
    let n010 = wh_hash31(i + vec3<f32>(0.0, 1.0, 0.0));
    let n110 = wh_hash31(i + vec3<f32>(1.0, 1.0, 0.0));
    let n001 = wh_hash31(i + vec3<f32>(0.0, 0.0, 1.0));
    let n101 = wh_hash31(i + vec3<f32>(1.0, 0.0, 1.0));
    let n011 = wh_hash31(i + vec3<f32>(0.0, 1.0, 1.0));
    let n111 = wh_hash31(i + vec3<f32>(1.0, 1.0, 1.0));
    let x00 = mix(n000, n100, u.x);
    let x10 = mix(n010, n110, u.x);
    let x01 = mix(n001, n101, u.x);
    let x11 = mix(n011, n111, u.x);
    return mix(mix(x00, x10, u.y), mix(x01, x11, u.y), u.z);
}
)wgsl";

// Verlauf entlang einer Richtung durch die Mitte (0.5, 0.5). Winkel in Grad.
constexpr const char* kGradientBody = R"wgsl(let a = radians(@param(angle));
let d = vec2<f32>(cos(a), sin(a));
let t = clamp(dot(p.xy - vec2<f32>(0.5, 0.5), d) + 0.5, 0.0, 1.0);
return wh_mix_color(@param(colorA), @param(colorB), t);)wgsl";

// Value Noise mit Reglern von 0 bis 1 (damit ein LFO sie direkt fahren kann):
//   scale    0..1 -> 1 bis 64 Zellen über die Leinwand (quadratisch, die untere Hälfte ist fein einstellbar)
//   speed    0..1 -> 0 bis 8 Zellen pro Sekunde durch die Zeitachse (quadratisch)
//   density  Anteil der Pixel, die an sind: 0 alles aus, 1 alles an, 0,5 die Hälfte, 0,25 ein Viertel ...
//   softness 0 harte Kante (reines Aus/An), 1 weicher Übergang bis zum glatten Rauschen
// Damit "Dichte" wirklich einen Anteil von Pixeln meint, wird der Rauschwert zuerst in seinen Rang
// (0..1, gleichverteilt) umgerechnet: Value-Noise häuft sich um 0,5, und ein fester Schwellwert träfe
// sonst nie den gewünschten Anteil. Die Streuung hängt von der Zeitphase ab (zwischen zwei Zeitschichten
// gemischt streut das Rauschen weniger, bei ganzzahliger Zeit am meisten); darum wird der Wert vor der
// Umrechnung mit der Phase auf gleiche Streuung gebracht. Die Tabelle ist die gemessene Verteilung dieses
// normierten Werts (Funktionswert bei -0,75 + k/16, k = 0..24), dazwischen wird linear gemischt; die
// Anteile liegen damit in jeder Phase auf etwa 2 % genau. Die Farbmischung steht absichtlich im Rumpf
// statt im gemeinsamen Farb-Prelude, damit der Node dasselbe Prelude wie "Fire" nutzen kann (gleiche
// Preludes werden nur einmal eingebunden).
constexpr const char* kNoiseBody =
    R"wgsl(let s = clamp(@param(scale), 0.0, 1.0);
let sp = clamp(@param(speed), 0.0, 1.0);
let q = p * (1.0 + 63.0 * s * s) + vec3<f32>(0.0, 0.0, @time * sp * sp * 8.0);
let fz = fract(q.z);
let w = fz * fz * (3.0 - 2.0 * fz);
let dev = (wh_value_noise(q) - 0.5) / sqrt((1.0 - w) * (1.0 - w) + w * w);
var cdf = array<f32, 25>(0.0, 0.0, 0.0001, 0.0006, 0.0031, 0.013, 0.0373, 0.0781, 0.1361, 0.2101, 0.2978, 0.3964, 0.5,
                         0.6036, 0.7022, 0.7898, 0.8639, 0.9219, 0.9627, 0.987, 0.9969, 0.9993, 0.9999, 1.0, 1.0);
let x = (clamp(dev, -0.75, 0.75) + 0.75) * 16.0;
let k = min(u32(floor(x)), 23u);
let rank = mix(cdf[k], cdf[k + 1u], x - f32(k));
let soft = max(clamp(@param(softness), 0.0, 1.0), 0.0001);
let m = clamp((clamp(@param(density), 0.0, 1.0) * (1.0 + soft) - rank) / soft, 0.0, 1.0);
let ca = @param(colorA);
let cb = @param(colorB);
let mixed = mix(vec4<f32>(ca.rgb * ca.a, ca.a), vec4<f32>(cb.rgb * cb.a, cb.a), m);
return vec4<f32>(mixed.rgb / max(mixed.a, 0.000001), mixed.a);)wgsl";

// Klassisches Plasma: Summe mehrerer Sinuswellen, in drei phasenverschobene Kanäle gelegt.
constexpr const char* kPlasmaBody =
    R"wgsl(let s = @param(scale);
let t = @time * @param(speed);
let q = p * s;
let v = sin(q.x + t) + sin(q.y * 1.3 + t * 1.1) + sin((q.x + q.y + t) * 0.7) + sin(length(q.xy - vec2<f32>(0.5 * s)) + t);
let w = v * 1.5707963;
return vec4<f32>(0.5 + 0.5 * sin(w), 0.5 + 0.5 * sin(w + 2.0943951), 0.5 + 0.5 * sin(w + 4.1887902), 1.0);)wgsl";

// Kugel im Raum: innen colorInside, außen colorOutside, dazwischen ein weicher Rand.
// Der Abstand wird in Einheitswürfel-Koordinaten gemessen; bei der Normalisierung
// "Stretch" wird eine Kugel in einer breiten Anlage deshalb zum Ellipsoid (KeepAspect
// hält sie rund). smoothstep braucht edge0 < edge1, daher die untere Grenze der Breite.
constexpr const char* kSphereBody =
    R"wgsl(let center = vec3<f32>(@param(cx), @param(cy), @param(cz));
let r = @param(radius);
let t = smoothstep(r, r + max(@param(softness), 0.0001), length(p - center));
return wh_mix_color(@param(colorInside), @param(colorOutside), t);)wgsl";

// Ebene Welle, die sich entlang einer Richtung bewegt. Die Richtung wird normiert;
// ein Nullvektor fällt auf die x-Achse zurück. Ein Wellenberg wandert mit positivem
// speed in Richtung des Vektors.
constexpr const char* kWaveBody =
    R"wgsl(let dv = vec3<f32>(@param(dirX), @param(dirY), @param(dirZ));
let len = length(dv);
let n = select(vec3<f32>(1.0, 0.0, 0.0), dv / max(len, 0.000001), len > 0.000001);
let phase = @param(frequency) * dot(p, n) - @param(speed) * @time;
let w = 0.5 + 0.5 * sin(6.2831853 * phase);
return wh_mix_color(@param(colorA), @param(colorB), w);)wgsl";

// "Digital Rain" (fallende Pixel wie im Film Matrix). Ohne Zustand: Jede Spalte (im Cube jede
// x/z-Säule) bekommt aus ihrer Nummer einen Zufallswert für Tempo, Phase und ob sie überhaupt
// einen Tropfen hat. Der Kopf wandert von oben (y = 1) nach unten, dahinter zieht sich die Spur
// nach oben und wird dabei durchsichtig (Alpha), so lässt sich der Regen über andere Effekte legen.
// Ein Durchlauf dauert (1 + Spurlänge) / Tempo; danach beginnt dieselbe Spalte von vorn.
constexpr const char* kRainPrelude = R"wgsl(fn wh_hash21(p: vec2<f32>) -> f32 {
    var q = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    q = q + dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
)wgsl";

constexpr const char* kRainBody = R"wgsl(let cols = max(floor(@param(columns)), 1.0);
let cell = floor(vec2<f32>(p.x, p.z) * cols);
let r1 = wh_hash21(cell);
let r2 = wh_hash21(cell + vec2<f32>(17.0, 3.0));
let r3 = wh_hash21(cell + vec2<f32>(5.0, 29.0));
let trail = max(@param(trail), 0.01);
let span = 1.0 + trail;
let travel = fract(r3 + @time * @param(speed) * mix(0.6, 1.4, r2) / span);
let headY = 1.0 - travel * span;
let behind = p.y - headY;
let strength = select(0.0, 1.0 - behind / trail, behind >= 0.0 && behind <= trail);
let on = select(0.0, 1.0, r1 < @param(density));
let heat = pow(strength, 4.0);
let head = @param(colorHead);
let tail = @param(colorTail);
return vec4<f32>(mix(tail.rgb, head.rgb, heat), mix(tail.a, head.a, heat) * strength * on);)wgsl";

// Fläche in einer Farbe, nützlich zum Einfärben (Mask, Blend) oder als Blitzfläche (Strobe).
constexpr const char* kSolidBody = R"wgsl(return @param(color);)wgsl";

// Scan-Linie (wie ein Radar oder Knight Rider): ein Streifen quer zur Richtung `angle`, an der Stelle
// `position` (0..1). Position mit einem LFO (Dreieck) gesteuert ergibt das Hin- und Herlaufen.
constexpr const char* kScanBody = R"wgsl(let a = radians(@param(angle));
let d = vec2<f32>(cos(a), sin(a));
let x = dot(p.xy - vec2<f32>(0.5, 0.5), d) + 0.5;
let w = max(@param(width), 0.0001);
let s = clamp(@param(softness), 0.01, 1.0);
let v = 1.0 - smoothstep(w * (1.0 - s), w, abs(x - @param(position)));
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

// Wellenringe wie Regentropfen im Wasser. Ohne Zustand: Tropfen Nummer k entsteht zur Zeit k / rate an
// einer Zufallsstelle, der Ring wächst mit `speed` und verblasst nach vier Perioden. Die letzten
// vier Tropfen überlagern sich.
constexpr const char* kRipplePrelude = R"wgsl(fn wh_hash11(x: f32) -> f32 {
    return fract(sin(x * 127.1) * 43758.5453);
}
)wgsl";

constexpr const char* kRippleBody = R"wgsl(let t = @time * max(@param(rate), 0.01);
var v = 0.0;
for (var i = 0; i < 4; i = i + 1) {
    let k = floor(t) - f32(i);
    let age = t - k;
    let c = vec2<f32>(wh_hash11(k * 1.37 + 0.11), wh_hash11(k * 2.71 + 5.31));
    let d = length(p.xy - c);
    let ring = 1.0 - smoothstep(0.0, max(@param(width), 0.001), abs(d - age * @param(speed)));
    v = v + ring * clamp(1.0 - age / 4.0, 0.0, 1.0);
}
let col = @param(color);
return vec4<f32>(col.rgb, col.a * clamp(v, 0.0, 1.0));)wgsl";

// Feuer: Rauschen, das nach oben wandert, dazu eine Hitze, die mit der Höhe abnimmt (unten y = 0 ist
// am heißesten). Farbverlauf von Schwarz über Rot und Gelb zu Weiß.
constexpr const char* kFireBody = R"wgsl(let t = @time * @param(speed);
let s = @param(scale);
let q = vec3<f32>(p.x * s, p.y * s * 0.6 - t, 0.0);
let n = wh_value_noise(q) * 0.65 + wh_value_noise(q * 2.0 + vec3<f32>(0.0, 0.0, 3.7)) * 0.35;
let heat = clamp(n * 1.4 + (1.0 - p.y / max(@param(height), 0.05)) - 0.5, 0.0, 1.0);
return vec4<f32>(clamp(heat * 2.0, 0.0, 1.0), clamp(heat * 2.0 - 0.7, 0.0, 1.0), clamp(heat * 3.0 - 2.0, 0.0, 1.0), 1.0);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> generatorNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode(
        "wh.generator.gradient", "Linear Gradient", "Generators", {fieldOutput()},
        {number("angle", "Angle (deg)", 0.0), color("colorA", "Color A", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kGradientBody, kColorPrelude));

    nodes.push_back(fieldNode("wh.generator.noise", "Value Noise", "Generators", {fieldOutput()},
                              {number("scale", "Scale", 0.33, 0.0, 1.0), number("speed", "Speed", 0.25, 0.0, 1.0),
                               number("density", "Density", 0.5, 0.0, 1.0), number("softness", "Softness", 0.5, 0.0, 1.0),
                               color("colorA", "Color A (off)", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
                               color("colorB", "Color B (on)", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
                              kNoiseBody, kNoisePrelude));

    nodes.push_back(fieldNode("wh.generator.plasma", "Plasma", "Generators", {fieldOutput()},
                              {number("scale", "Scale", 3.0, 0.1, 64.0),
                               number("speed", "Speed", 1.0, -20.0, 20.0)},
                              kPlasmaBody));

    nodes.push_back(fieldNode(
        "wh.generator.sphere", "Sphere", "Generators", {fieldOutput()},
        {number("cx", "Center X", 0.5, 0.0, 1.0), number("cy", "Center Y", 0.5, 0.0, 1.0),
         number("cz", "Center Z", 0.5, 0.0, 1.0), number("radius", "Radius", 0.3, 0.0, 2.0),
         number("softness", "Softness", 0.1, 0.0, 1.0),
         color("colorInside", "Inside", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         color("colorOutside", "Outside", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F})},
        kSphereBody, kColorPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.wave", "Plane Wave", "Generators", {fieldOutput()},
        {number("dirX", "Direction X", 1.0, -1.0, 1.0), number("dirY", "Direction Y", 0.0, -1.0, 1.0),
         number("dirZ", "Direction Z", 0.0, -1.0, 1.0), number("frequency", "Frequency", 2.0, 0.0, 64.0),
         number("speed", "Speed", 0.5, -20.0, 20.0),
         color("colorA", "Color A", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kWaveBody, kColorPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.rain", "Digital Rain", "Generators", {fieldOutput()},
        {number("columns", "Columns", 16.0, 1.0, 256.0), number("speed", "Speed", 0.6, 0.0, 10.0),
         number("trail", "Trail length", 0.5, 0.05, 4.0), number("density", "Density", 0.7, 0.0, 1.0),
         color("colorHead", "Head", ColorRgba{0.8F, 1.0F, 0.85F, 1.0F}),
         color("colorTail", "Tail", ColorRgba{0.0F, 0.6F, 0.05F, 1.0F})},
        kRainBody, kRainPrelude));

    nodes.push_back(fieldNode("wh.generator.solid", "Solid Color", "Generators", {fieldOutput()},
                              {color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})}, kSolidBody));

    nodes.push_back(fieldNode(
        "wh.generator.scan", "Scan Line", "Generators", {fieldOutput()},
        {number("position", "Position", 0.5, 0.0, 1.0), number("width", "Width", 0.08, 0.001, 1.0),
         number("softness", "Softness", 0.5, 0.0, 1.0), number("angle", "Angle (deg)", 0.0),
         color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kScanBody));

    nodes.push_back(fieldNode(
        "wh.generator.ripples", "Ripples", "Generators", {fieldOutput()},
        {number("rate", "Drops per second", 0.7, 0.01, 20.0), number("speed", "Spread speed", 0.25, 0.0, 5.0),
         number("width", "Ring width", 0.05, 0.001, 0.5), color("color", "Color", ColorRgba{0.3F, 0.7F, 1.0F, 1.0F})},
        kRippleBody, kRipplePrelude));

    nodes.push_back(fieldNode("wh.generator.fire", "Fire", "Generators", {fieldOutput()},
                              {number("scale", "Scale", 4.0, 0.1, 64.0), number("speed", "Speed", 1.5, -20.0, 20.0),
                               number("height", "Flame height", 0.8, 0.05, 2.0)},
                              kFireBody, kNoisePrelude));

    return nodes;
}

}  // namespace whitehole::nodes::detail
