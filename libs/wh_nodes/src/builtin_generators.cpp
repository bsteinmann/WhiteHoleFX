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

constexpr const char* kNoiseBody =
    R"wgsl(let v = wh_value_noise(p * @param(scale) + vec3<f32>(0.0, 0.0, @time * @param(speed)));
return vec4<f32>(v, v, v, 1.0);)wgsl";

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

}  // namespace

std::vector<compiler::NodeDefinition> generatorNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode(
        "wh.generator.gradient", "Linear Gradient", "Generators", {fieldOutput()},
        {number("angle", "Angle (deg)", 0.0), color("colorA", "Color A", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kGradientBody, kColorPrelude));

    nodes.push_back(fieldNode("wh.generator.noise", "Value Noise", "Generators", {fieldOutput()},
                              {number("scale", "Scale", 8.0, 0.01, 256.0),
                               number("speed", "Speed", 0.5, -20.0, 20.0)},
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

    return nodes;
}

}  // namespace whitehole::nodes::detail
