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
return mix(@param(colorA), @param(colorB), t);)wgsl";

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
return mix(@param(colorInside), @param(colorOutside), t);)wgsl";

// Ebene Welle, die sich entlang einer Richtung bewegt. Die Richtung wird normiert;
// ein Nullvektor fällt auf die x-Achse zurück. Ein Wellenberg wandert mit positivem
// speed in Richtung des Vektors.
constexpr const char* kWaveBody =
    R"wgsl(let dv = vec3<f32>(@param(dirX), @param(dirY), @param(dirZ));
let len = length(dv);
let n = select(vec3<f32>(1.0, 0.0, 0.0), dv / max(len, 0.000001), len > 0.000001);
let phase = @param(frequency) * dot(p, n) - @param(speed) * @time;
let w = 0.5 + 0.5 * sin(6.2831853 * phase);
return mix(@param(colorA), @param(colorB), w);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> generatorNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode(
        "wh.generator.gradient", "Linear Gradient", "Generators", {fieldOutput()},
        {number("angle", "Angle (deg)", 0.0), color("colorA", "Color A", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kGradientBody));

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
        kSphereBody));

    nodes.push_back(fieldNode(
        "wh.generator.wave", "Plane Wave", "Generators", {fieldOutput()},
        {number("dirX", "Direction X", 1.0, -1.0, 1.0), number("dirY", "Direction Y", 0.0, -1.0, 1.0),
         number("dirZ", "Direction Z", 0.0, -1.0, 1.0), number("frequency", "Frequency", 2.0, 0.0, 64.0),
         number("speed", "Speed", 0.5, -20.0, 20.0),
         color("colorA", "Color A", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F})},
        kWaveBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
