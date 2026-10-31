// Mischer: verrechnen zwei Felder an derselben Koordinate.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kMixBody =
    R"wgsl(return wh_mix_color(@in(a, p), @in(b, p), clamp(@param(amount), 0.0, 1.0));)wgsl";

constexpr const char* kAddBody =
    R"wgsl(return wh_add_color(@in(a, p), @in(b, p));)wgsl";

// "Blend": B liegt über A, die Farben verrechnen sich nach dem gewählten Modus. Gerechnet wird wie in
// der üblichen Bildverarbeitung: erst die Farbe aus beiden (nur wo A deckt), dann B mit seinem Alpha
// über A. `amount` überblendet zwischen A (0) und dem Ergebnis (1).
constexpr const char* kBlendBody = R"wgsl(let a = @in(a, p);
let b = @in(b, p);
let m = @param(mode);
var c = b.rgb;
if (m < 0.5) {
    c = min(a.rgb + b.rgb, vec3<f32>(1.0, 1.0, 1.0));
} else if (m < 1.5) {
    c = a.rgb * b.rgb;
} else if (m < 2.5) {
    c = vec3<f32>(1.0, 1.0, 1.0) - (vec3<f32>(1.0, 1.0, 1.0) - a.rgb) * (vec3<f32>(1.0, 1.0, 1.0) - b.rgb);
} else if (m < 3.5) {
    c = max(a.rgb - b.rgb, vec3<f32>(0.0, 0.0, 0.0));
} else if (m < 4.5) {
    c = abs(a.rgb - b.rgb);
} else if (m < 5.5) {
    c = max(a.rgb, b.rgb);
} else if (m < 6.5) {
    c = min(a.rgb, b.rgb);
}
let shown = (1.0 - a.a) * b.rgb + a.a * c;
let alpha = b.a + a.a * (1.0 - b.a);
let light = b.a * shown + (1.0 - b.a) * a.a * a.rgb;
let result = vec4<f32>(light / max(alpha, 0.000001), alpha);
return wh_mix_color(a, result, clamp(@param(amount), 0.0, 1.0));)wgsl";

// "Crossfade": Linear blendet A aus, während B einblendet. Progressive lässt beide in der Mitte
// voll leuchten (Summe, wie ein Überblenden zweier Lichtkanäle).
constexpr const char* kCrossfadeBody = R"wgsl(let t = clamp(@param(amount), 0.0, 1.0);
let a = @in(a, p);
let b = @in(b, p);
var wa = 1.0 - t;
var wb = t;
if (@param(mode) > 0.5) {
    wa = min(1.0, 2.0 * (1.0 - t));
    wb = min(1.0, 2.0 * t);
}
return wh_add_color(vec4<f32>(a.rgb, a.a * wa), vec4<f32>(b.rgb, b.a * wb));)wgsl";

// "Mask": Die Helligkeit der Maske bestimmt, wie viel vom Inhalt zu sehen ist. "Shape" macht daraus
// eine harte Form (Schwelle), "Intensity" behält Verläufe und weiche Ränder.
constexpr const char* kMaskBody = R"wgsl(let c = @in(content, p);
let m = @in(mask, p);
var v = dot(m.rgb, vec3<f32>(0.299, 0.587, 0.114)) * m.a;
if (@param(mode) < 0.5) {
    v = smoothstep(@param(threshold), @param(threshold) + max(@param(softness), 0.0001), v);
}
if (@param(invert) > 0.5) {
    v = 1.0 - v;
}
return vec4<f32>(c.rgb, c.a * v);)wgsl";

// "Key": Schwarz wird durchsichtig (Alpha folgt der Helligkeit), danach lässt sich das Bild mit
// Blend (Over) oder Add über etwas anderes legen.
constexpr const char* kKeyBody = R"wgsl(let c = @in(source, p);
let v = smoothstep(@param(threshold), @param(threshold) + max(@param(softness), 0.0001), max(c.r, max(c.g, c.b)));
return vec4<f32>(c.rgb, c.a * v);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> blendNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode("wh.blend.mix", "Mix", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()},
                              {number("amount", "Amount", 0.5, 0.0, 1.0)}, kMixBody, kColorPrelude));

    nodes.push_back(fieldNode("wh.blend.add", "Add", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()}, {},
                              kAddBody, kColorPrelude));

    nodes.push_back(fieldNode(
        "wh.blend.blend", "Blend", "Blend", {fieldInput("a", "A (below)"), fieldInput("b", "B (above)"), fieldOutput()},
        {choice("mode", "Mode", {"Add", "Multiply", "Screen", "Subtract", "Difference", "Lighten", "Darken", "Over"}),
         number("amount", "Amount", 1.0, 0.0, 1.0)},
        kBlendBody, kColorPrelude));

    nodes.push_back(fieldNode("wh.blend.crossfade", "Crossfade", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()},
                              {choice("mode", "Mode", {"Linear", "Progressive"}),
                               number("amount", "Position", 0.5, 0.0, 1.0)},
                              kCrossfadeBody, kColorPrelude));

    nodes.push_back(fieldNode("wh.blend.mask", "Mask", "Blend",
                              {fieldInput("content", "Content"), fieldInput("mask", "Mask"), fieldOutput()},
                              {choice("mode", "Mode", {"Shape (hard edge)", "Intensity (keeps fades)"}, 1),
                               number("threshold", "Threshold", 0.05, 0.0, 1.0), number("softness", "Softness", 0.05, 0.0, 1.0),
                               toggle("invert", "Invert mask", false)},
                              kMaskBody));

    nodes.push_back(fieldNode("wh.blend.key", "Key", "Blend", {fieldInput("source", "Source"), fieldOutput()},
                              {number("threshold", "Threshold", 0.02, 0.0, 1.0),
                               number("softness", "Softness", 0.1, 0.0, 1.0)},
                              kKeyBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
