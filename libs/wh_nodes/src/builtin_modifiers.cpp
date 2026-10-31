// Modifier und Ausgang: ändern das Bild eines Eingangs (Farbe, Blitzen, Umriss) und legen es auf die
// Leinwand. Wie alle Field-Nodes laufen sie als WGSL-Ausschnitt in der Effektkette, ohne Zwischenbild.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kHsvPrelude = R"wgsl(fn wh_rgb2hsv(c: vec3<f32>) -> vec3<f32> {
    let k = vec4<f32>(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    let a = mix(vec4<f32>(c.b, c.g, k.w, k.z), vec4<f32>(c.g, c.b, k.x, k.y), step(c.b, c.g));
    let b = mix(vec4<f32>(a.x, a.y, a.w, c.r), vec4<f32>(c.r, a.y, a.z, a.x), step(a.x, c.r));
    let d = b.x - min(b.w, b.y);
    let e = 1.0e-10;
    return vec3<f32>(abs(b.z + (b.w - b.y) / (6.0 * d + e)), d / (b.x + e), b.x);
}

fn wh_hsv2rgb(c: vec3<f32>) -> vec3<f32> {
    let k = vec4<f32>(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    let q = abs(fract(vec3<f32>(c.x, c.x, c.x) + k.xyz) * 6.0 - vec3<f32>(k.w, k.w, k.w));
    return c.z * mix(vec3<f32>(k.x, k.x, k.x), clamp(q - vec3<f32>(k.x, k.x, k.x), vec3<f32>(0.0, 0.0, 0.0), vec3<f32>(1.0, 1.0, 1.0)), c.y);
}
)wgsl";

// Farbton (in Umläufen), Sättigung und Helligkeit, danach Gamma und optional Invertieren.
constexpr const char* kColorAdjustBody = R"wgsl(let c = @in(source, p);
var h = wh_rgb2hsv(c.rgb);
h.x = fract(h.x + @param(hue));
h.y = clamp(h.y * @param(saturation), 0.0, 1.0);
h.z = clamp(h.z * @param(brightness), 0.0, 1.0);
var rgb = wh_hsv2rgb(h);
let g = 1.0 / max(@param(gamma), 0.01);
rgb = vec3<f32>(pow(rgb.x, g), pow(rgb.y, g), pow(rgb.z, g));
if (@param(invert) > 0.5) {
    rgb = vec3<f32>(1.0, 1.0, 1.0) - rgb;
}
return vec4<f32>(rgb, c.a);)wgsl";

// Blitzen: `rate` Blitze pro Sekunde, jeder leuchtet `duty` einer Periode und klingt danach über `fade`
// (ebenfalls Anteil einer Periode) aus. In der Dunkelphase wird das Alpha kleiner, die LEDs gehen aus.
constexpr const char* kStrobeBody = R"wgsl(let c = @in(source, p);
let phase = fract(@time * @param(rate));
let duty = clamp(@param(duty), 0.0, 1.0);
var env = 1.0;
if (phase >= duty) {
    env = clamp(1.0 - (phase - duty) / max(@param(fade), 0.0001), 0.0, 1.0);
}
return vec4<f32>(c.rgb, c.a * env);)wgsl";

// Umriss: Wo das Bild dunkel ist, aber in der Nähe (Abstand `width`, in acht Richtungen geprüft) etwas
// Helles liegt, wird die Randfarbe gezeichnet. So bekommt Text oder eine Form einen Rand.
constexpr const char* kOutlineBody = R"wgsl(let c = @in(source, p);
let w = max(@param(width), 0.0001);
var near = 0.0;
for (var i = 0; i < 8; i = i + 1) {
    let ang = f32(i) * 0.7853982;
    let s = @in(source, vec3<f32>(p.x + cos(ang) * w, p.y + sin(ang) * w, p.z));
    near = max(near, max(s.r, max(s.g, s.b)) * s.a);
}
let own = max(c.r, max(c.g, c.b)) * c.a;
let edge = clamp((near - own) * 4.0, 0.0, 1.0) * step(@param(threshold), near);
let line = @param(color);
let k = edge * line.a;
return vec4<f32>(mix(c.rgb, line.rgb, k), max(c.a, k));)wgsl";

// Ausgang: bespielt die ganze Leinwand oder nur ein Rechteck (Anteile 0..1, `top` zählt von oben).
// "Scale into area" bildet das Eingangsbild auf das Rechteck ab, "Window" lässt das Bild an seinem Platz
// und zeigt es nur im Rechteck. Außerhalb ist es durchsichtig, diese LEDs bleiben aus. `brightness`
// ist die Gesamthelligkeit.
constexpr const char* kOutputBody = R"wgsl(let w = max(@param(width), 0.0001);
let h = max(@param(height), 0.0001);
let u = (p.x - @param(left)) / w;
let v = (p.y - (1.0 - @param(top) - h)) / h;
if (u < -0.0001 || u > 1.0001 || v < -0.0001 || v > 1.0001) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
}
var q = p;
if (@param(mode) < 0.5) {
    q = vec3<f32>(u, v, p.z);
}
let c = @in(source, q);
let level = clamp(@param(brightness), 0.0, 1.0);
return vec4<f32>(c.rgb * level, c.a);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> modifierNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode("wh.modifier.coloradjust", "Color Adjust", "Modifier",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("hue", "Hue shift (turns)", 0.0), number("saturation", "Saturation", 1.0, 0.0, 4.0),
                               number("brightness", "Brightness", 1.0, 0.0, 4.0), number("gamma", "Gamma", 1.0, 0.1, 5.0),
                               toggle("invert", "Invert", false)},
                              kColorAdjustBody, kHsvPrelude));

    nodes.push_back(fieldNode("wh.modifier.strobe", "Strobe", "Modifier",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("rate", "Rate (Hz)", 4.0, 0.05, 60.0), number("duty", "On time (of period)", 0.3, 0.0, 1.0),
                               number("fade", "Fade out (of period)", 0.0, 0.0, 1.0)},
                              kStrobeBody));

    nodes.push_back(fieldNode("wh.modifier.outline", "Outline", "Modifier",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("width", "Width", 0.04, 0.001, 0.5), number("threshold", "Threshold", 0.1, 0.0, 1.0),
                               color("color", "Outline color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
                              kOutlineBody));

    nodes.push_back(fieldNode("wh.output.canvas", "Output", "Output", {fieldInput("source", "Source"), fieldOutput()},
                              {choice("mode", "Mode", {"Scale into area", "Window (cut out)"}),
                               number("left", "Left", 0.0, 0.0, 1.0), number("top", "Top", 0.0, 0.0, 1.0),
                               number("width", "Width", 1.0, 0.0, 1.0), number("height", "Height", 1.0, 0.0, 1.0),
                               number("brightness", "Brightness", 1.0, 0.0, 1.0)},
                              kOutputBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
