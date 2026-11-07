// Generatoren für 1D-Effekte (builtin_generators_1d.cpp): Lichtbänder entlang einer Achse, gedacht für Strips.
//
// Warum eine eigene Gruppe "Generators 1D": Ein Strip hat nur eine Richtung. Die 2D-Generatoren (Gradient, Plasma ...)
// sind dafür unnötig umständlich; diese hier bringen Lauflicht, Meteor, Funkeln und Pegelbalken fertig mit. Sie rechnen
// mit EINER Koordinate (0..1 entlang der gewählten Achse x, y oder z), laufen also auf jedem Fixture, das in dieser
// Richtung ausgedehnt ist (Strip, Säule, Matrix, Cube), und sind auf einem senkrecht stehenden Strip mit Achse y genauso
// zu Hause wie auf einem liegenden mit x.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Gemeinsamer Vorspann: die Koordinate entlang der Achse, ein Zufallswert aus einer Zahl und das Mischen zweier Farben.
constexpr const char* k1dPrelude = R"wgsl(fn wh_axis_u(p: vec3<f32>, axis: f32) -> f32 {
    let a = i32(floor(axis + 0.5));
    if (a == 1) {
        return p.y;
    }
    if (a == 2) {
        return p.z;
    }
    return p.x;
}

fn wh_hash_1d(x: f32) -> f32 {
    return fract(sin(x * 12.9898 + 78.233) * 43758.5453);
}

fn wh_lerp_1d(a: vec4<f32>, b: vec4<f32>, t: f32) -> vec4<f32> {
    return vec4<f32>(mix(a.rgb, b.rgb, t), mix(a.a, b.a, t));
}
)wgsl";

// Komet: ein Kopf läuft die Achse entlang, dahinter ein Schweif, der zum Ende hin verblasst. Ohne "Ping-pong" beginnt
// er am Ende wieder von vorn (der Schweif läuft dann um die Ecke), mit "Ping-pong" kehrt er an den Enden um.
// `speed` ist die Zahl der Durchläufe pro Sekunde, negativ läuft rückwärts.
constexpr const char* kCometBody = R"wgsl(let u = wh_axis_u(p, @param(axis));
let phase = fract(@param(speed) * @time);
let dir = select(-1.0, 1.0, @param(speed) >= 0.0);
let len = max(@param(length), 0.001);
var head = phase;
var d = 0.0;
if (@param(bounce) > 0.5) {
    head = 1.0 - abs(2.0 * phase - 1.0);
    let way = select(-dir, dir, phase < 0.5);
    d = (head - u) * way;
} else {
    d = fract((head - u) * dir);
}
let tail = select(0.0, pow(clamp(1.0 - d / len, 0.0, 1.0), 2.0), d >= 0.0 && d < len);
let col = @param(color);
return vec4<f32>(col.rgb, col.a * tail);)wgsl";

// Pulswellen: Bänder, die über die Achse laufen. `sharpness` macht aus der weichen Sinuswelle schmale Pulse.
constexpr const char* kPulseBody = R"wgsl(let u = wh_axis_u(p, @param(axis));
let phase = @param(frequency) * u - @param(speed) * @time;
let w = 0.5 + 0.5 * sin(6.2831853 * phase);
let v = pow(w, 1.0 + 9.0 * clamp(@param(sharpness), 0.0, 1.0));
return wh_lerp_1d(@param(colorA), @param(colorB), v);)wgsl";

// Verlauf über drei Farben A -> B -> C -> A, `repeat` Mal über die Länge, `speed` Durchläufe pro Sekunde.
constexpr const char* kStripGradientBody = R"wgsl(let u = wh_axis_u(p, @param(axis));
let t = fract(u * max(@param(repeat), 0.0) - @param(speed) * @time) * 3.0;
let a = @param(colorA);
let b = @param(colorB);
let c = @param(colorC);
if (t < 1.0) {
    return wh_lerp_1d(a, b, t);
}
if (t < 2.0) {
    return wh_lerp_1d(b, c, t - 1.0);
}
return wh_lerp_1d(c, a, t - 2.0);)wgsl";

// Meteore: bis zu acht Kometen, jeder mit eigener Geschwindigkeit und eigenem Versatz (aus dem Index gewürfelt).
constexpr const char* kMeteorBody = R"wgsl(let u = wh_axis_u(p, @param(axis));
let dir = select(-1.0, 1.0, @param(speed) >= 0.0);
let len = max(@param(length), 0.001);
var v = 0.0;
for (var i = 0; i < 8; i = i + 1) {
    if (f32(i) >= @param(count)) {
        break;
    }
    let fi = f32(i);
    let rate = 0.6 + 0.8 * wh_hash_1d(fi + 3.0);
    let head = fract(@param(speed) * rate * @time + wh_hash_1d(fi + 11.0));
    let d = fract((head - u) * dir);
    if (d < len) {
        v = max(v, pow(clamp(1.0 - d / len, 0.0, 1.0), 2.0));
    }
}
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

// Funkeln: `cells` Zellen entlang der Achse (zum Beispiel die LED-Zahl); in einem Teil davon (`density`) blitzt es auf
// und klingt ab, jede Zelle mit eigenem Takt.
constexpr const char* kSparkleBody = R"wgsl(let u = clamp(wh_axis_u(p, @param(axis)), 0.0, 0.9999);
let n = max(floor(@param(cells)), 1.0);
let cell = floor(u * n);
let h1 = wh_hash_1d(cell);
let h2 = wh_hash_1d(cell + 17.0);
let on = select(0.0, 1.0, h1 < @param(density));
let ph = fract(@time * @param(speed) * (0.5 + h2) + h1 * 7.0);
let v = on * pow(1.0 - ph, 4.0);
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

// Pegelbalken: füllt die Achse bis `level` (0..1), unten `colorLow`, oben `colorHigh`. `level` ist ein Eingang und kommt
// zum Beispiel von "Audio Level". `flip` füllt vom anderen Ende.
constexpr const char* kLevelBarBody = R"wgsl(let raw = wh_axis_u(p, @param(axis));
let u = select(raw, 1.0 - raw, @param(flip) > 0.5);
let edge = max(@param(softness) * 0.1, 0.002);
let inside = 1.0 - smoothstep(@param(level) - edge, @param(level), u);
let col = wh_lerp_1d(@param(colorLow), @param(colorHigh), u);
return vec4<f32>(col.rgb, col.a * inside);)wgsl";

graph::ParameterDescriptor axisChoice() { return choice("axis", "Axis", {"X", "Y", "Z"}, 0); }

}  // namespace

std::vector<compiler::NodeDefinition> generators1dNodes() {
    std::vector<compiler::NodeDefinition> nodes;
    const char* group = "Generators 1D";

    nodes.push_back(fieldNode(
        "wh.generator1d.comet", "Comet", group, {fieldOutput()},
        {axisChoice(), number("speed", "Speed (passes per second)", 0.5, -10.0, 10.0),
         number("length", "Tail length", 0.25, 0.01, 1.0), toggle("bounce", "Ping-pong", false),
         color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kCometBody, k1dPrelude));

    nodes.push_back(fieldNode(
        "wh.generator1d.pulses", "Pulse Waves", group, {fieldOutput()},
        {axisChoice(), number("frequency", "Frequency (waves along the strip)", 3.0, 0.0, 64.0),
         number("speed", "Speed", 0.5, -20.0, 20.0), number("sharpness", "Sharpness", 0.3, 0.0, 1.0),
         color("colorA", "Color A", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kPulseBody, k1dPrelude));

    nodes.push_back(fieldNode(
        "wh.generator1d.gradient", "Strip Gradient", group, {fieldOutput()},
        {axisChoice(), number("repeat", "Repeat", 1.0, 0.0, 16.0), number("speed", "Scroll speed", 0.0, -5.0, 5.0),
         color("colorA", "Color A", ColorRgba{1.0F, 0.0F, 0.3F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.0F, 0.4F, 1.0F, 1.0F}),
         color("colorC", "Color C", ColorRgba{1.0F, 0.8F, 0.0F, 1.0F})},
        kStripGradientBody, k1dPrelude));

    nodes.push_back(fieldNode(
        "wh.generator1d.meteors", "Meteors", group, {fieldOutput()},
        {axisChoice(), integer("count", "Count", 3, 1, 8), number("speed", "Speed (passes per second)", 0.4, -10.0, 10.0),
         number("length", "Tail length", 0.2, 0.01, 1.0), color("color", "Color", ColorRgba{0.7F, 0.85F, 1.0F, 1.0F})},
        kMeteorBody, k1dPrelude));

    nodes.push_back(fieldNode(
        "wh.generator1d.sparkle", "Sparkle", group, {fieldOutput()},
        {axisChoice(), number("cells", "Cells (e.g. LED count)", 60.0, 1.0, 1024.0),
         number("density", "Density", 0.3, 0.0, 1.0), number("speed", "Speed", 1.0, 0.0, 10.0),
         color("color", "Color", ColorRgba{1.0F, 0.95F, 0.8F, 1.0F})},
        kSparkleBody, k1dPrelude));

    nodes.push_back(fieldNode(
        "wh.generator1d.levelbar", "Level Bar", group, {fieldOutput()},
        {axisChoice(), number("level", "Level", 0.5, 0.0, 1.0), number("softness", "Edge softness", 0.2, 0.0, 1.0),
         toggle("flip", "Fill from the other end", false),
         color("colorLow", "Color low", ColorRgba{0.0F, 1.0F, 0.2F, 1.0F}),
         color("colorHigh", "Color high", ColorRgba{1.0F, 0.1F, 0.0F, 1.0F})},
        kLevelBarBody, k1dPrelude));

    return nodes;
}

}  // namespace whitehole::nodes::detail
