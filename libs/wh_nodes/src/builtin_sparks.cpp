// Blitze und Partikel: Lightning und Particles.
//
// Warum eine eigene Datei: Beide Nodes sind Effekte mit Zufall *und* Bewegung und teilen ein
// Prelude (Hash, 1D-Rauschen, Blitzpfad, Abstand zu einer Strecke). builtin_shapes.cpp bleibt damit
// bei den ruhigen Mustern.
//
// Beide Nodes sind zustandslos, wie alle Field-Nodes: Das Bild hängt nur von Koordinate und Zeit ab.
// Es gibt keine Partikelliste, die von Frame zu Frame fortgeschrieben wird. Stattdessen hat jeder
// Platz (Blitz-Slot oder Partikel i) einen eigenen Takt; in jedem Durchlauf würfelt ein Hash
// Startpunkt, Richtung und Tempo neu, und die Bahn steht in geschlossener Form da (Ort = Start +
// Geschwindigkeit * Alter + Schwerkraft * Alter^2 / 2). Dasselbe Bild kommt bei jedem Abspielen und
// auf jeder Leinwandgröße heraus, und der Compiler muss nichts über Zustand wissen.
//
// Koordinaten: p.y = 0 ist unten, Blitze hängen also an p.y = 1 (oben) und gehen nach unten.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Gemeinsames Prelude (siehe builtin_shapes.cpp: gleiche Preludes werden zusammengelegt, darum hat es
// eigene Namen und darf nicht wh_cell_hash anderer Nodes mitbringen). Zyklen werden mit 997
// abgeschnitten, damit der Hash nach Tagen Laufzeit nicht an Float-Genauigkeit verliert.
constexpr const char* kSparkPrelude = R"wgsl(fn wh_spark_hash(p: vec2<f32>) -> f32 {
    var q = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    q = q + dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

fn wh_spark_noise(x: f32, seed: f32) -> f32 {
    let i = floor(x);
    let f = fract(x);
    let u = f * f * (3.0 - 2.0 * f);
    return mix(wh_spark_hash(vec2<f32>(i, seed)), wh_spark_hash(vec2<f32>(i + 1.0, seed)), u) * 2.0 - 1.0;
}

fn wh_bolt_x(dy: f32, x0: f32, seed: f32, jag: f32) -> f32 {
    let k = dy * 4.0;
    let d = wh_spark_noise(k, seed) * 0.6 + wh_spark_noise(k * 2.7, seed + 5.0) * 0.3 + wh_spark_noise(k * 7.3, seed + 9.0) * 0.1;
    return x0 + d * jag * smoothstep(0.0, 0.08, dy);
}

fn wh_seg_dist(p: vec2<f32>, a: vec2<f32>, b: vec2<f32>) -> f32 {
    let ab = b - a;
    let t = clamp(dot(p - a, ab) / max(dot(ab, ab), 0.0000001), 0.0, 1.0);
    return length(p - (a + ab * t));
}
)wgsl";

// Lightning: bis zu drei Blitz-Plätze. Jeder hat einen Takt von `rate` Schlägen pro Sekunde; im Takt
// liegt der Blitz an einer zufälligen Stelle und sichtbar ist er `duration` Sekunden lang. Der Pfad
// ist die Summe dreier Rauschschichten (grob bis fein), oben an der Wolke verankert. Die Helligkeit
// klingt mit `decay` ab; eine zweite Spitze bei etwa 30 % der Sichtbarkeit ist der Nachschlag. `branches`
// ist die Helligkeit eines Seitenastes (0 = keiner), `reach` wie weit der Blitz nach unten läuft,
// `flash` hellt die ganze Fläche mit auf. Das Flackern ist ein schnelles Sinusrauschen auf dem Abklingen.
constexpr const char* kLightningBody = R"wgsl(let n = i32(clamp(floor(@param(bolts)), 1.0, 3.0));
let rate = max(@param(rate), 0.01);
let life = clamp(@param(duration) * rate, 0.02, 1.0);
let dy = 1.0 - p.y;
let col = @param(color);
let w = max(@param(width), 0.002);
let jag = @param(jag);
let reach = clamp(@param(reach), 0.05, 1.0);
let br = clamp(@param(branches), 0.0, 1.0);
var light = vec3<f32>(0.0, 0.0, 0.0);
var asum = 0.0;
for (var i = 0; i < 3; i = i + 1) {
    if (i < n) {
        let fi = f32(i);
        let t = @time * rate * (1.0 + 0.17 * fi) + fi * 0.37;
        let whole = floor(t);
        let ev = whole - 997.0 * floor(whole / 997.0);
        let h0 = wh_spark_hash(vec2<f32>(ev, fi * 17.0 + 1.0));
        let h1 = wh_spark_hash(vec2<f32>(ev, fi * 17.0 + 2.0));
        let h2 = wh_spark_hash(vec2<f32>(ev, fi * 17.0 + 3.0));
        let h3 = wh_spark_hash(vec2<f32>(ev, fi * 17.0 + 4.0));
        let lt = (fract(t) - h1 * (1.0 - life)) / life;
        if (lt >= 0.0 && lt < 1.0) {
            let restrike = 0.8 * exp(-pow((lt - 0.3) * 7.0, 2.0));
            let strength = max(pow(1.0 - lt, max(@param(decay), 0.1)), restrike) * (0.8 + 0.2 * sin(lt * 40.0 + h0 * 6.2831853));
            let seed = ev * 3.17 + fi * 11.0;
            let x0 = mix(0.15, 0.85, h0);
            let xc = wh_bolt_x(dy, x0, seed, jag);
            let d = abs(p.x - xc);
            let halo = @param(glow) * exp(-d / (w * 4.0));
            var v = clamp(1.0 - smoothstep(0.0, w, d) + halo, 0.0, 1.0) * (1.0 - smoothstep(reach - 0.15, reach, dy));
            if (br > 0.0) {
                let dyb = mix(0.15, 0.55, h2) * reach;
                let side = select(-1.0, 1.0, h3 > 0.5);
                let xb = wh_bolt_x(dyb, x0, seed, jag) + side * (dy - dyb) * 0.6
                       + wh_spark_noise(dy * 9.0, seed + 21.0) * jag * 0.3 * smoothstep(dyb, dyb + 0.05, dy);
                let blen = 0.3 * reach;
                let bamp = smoothstep(dyb, dyb + 0.02, dy) * (1.0 - smoothstep(dyb + blen * 0.5, dyb + blen, dy));
                let db = abs(p.x - xb);
                let bv = clamp(1.0 - smoothstep(0.0, w * 0.7, db) + halo * 0.5, 0.0, 1.0) * bamp * br * 0.8;
                v = max(v, bv);
            }
            let a = col.a * strength * v;
            let fl = col.a * strength * clamp(@param(flash), 0.0, 1.0);
            light = light + col.rgb * (a + fl);
            asum = asum + a + fl;
        }
    }
}
let alpha = clamp(asum, 0.0, 1.0);
return vec4<f32>(min(light / max(alpha, 0.000001), vec3<f32>(1.0, 1.0, 1.0)), alpha);)wgsl";

// Particles: `count` Teilchen, gleichmäßig über `lifetime` Sekunden gestaffelt, strömen aus dem
// Ausgangspunkt (ex, ey). Richtung `direction` in Grad (0 = rechts, 90 = oben), der Fächer `spread`
// in Umläufen (1 = rundum). Die Bahn ist eine Wurfparabel mit `gravity` nach unten. Jedes Teilchen
// wird als Strecke vom Ort vor `trail` Sekunden bis zum jetzigen gezeichnet (Schweif), schrumpft um
// `shrink` und verblasst über sein Leben. Palette: Mischung aus A und B nach Zufall oder Regenbogen.
constexpr const char* kParticlesBody = R"wgsl(let n = i32(clamp(floor(@param(count)), 1.0, 48.0));
let life = max(@param(lifetime), 0.05);
let em = vec2<f32>(@param(ex), @param(ey));
let dir0 = radians(@param(direction));
let spread = @param(spread) * 6.2831853;
let g = vec2<f32>(0.0, -@param(gravity));
let sz = max(@param(size), 0.002);
let trail = max(@param(trail), 0.0);
let shrink = clamp(@param(shrink), 0.0, 1.0);
let rainbow = i32(floor(@param(palette) + 0.5)) == 1;
let ca = @param(colorA);
let cb = @param(colorB);
var light = vec3<f32>(0.0, 0.0, 0.0);
var asum = 0.0;
for (var i = 0; i < 48; i = i + 1) {
    if (i < n) {
        let fi = f32(i);
        let u = @time / life + fi / f32(n);
        let whole = floor(u);
        let cyc = whole - 997.0 * floor(whole / 997.0);
        let age = fract(u) * life;
        let key = fi + cyc * 64.0;
        let ha = wh_spark_hash(vec2<f32>(key, 1.0));
        let hb = wh_spark_hash(vec2<f32>(key, 2.0));
        let hc = wh_spark_hash(vec2<f32>(key, 3.0));
        let ang = dir0 + (ha - 0.5) * spread;
        let vel = vec2<f32>(cos(ang), sin(ang)) * (@param(speed) * mix(0.4, 1.0, hb));
        let pos = em + vel * age + 0.5 * g * age * age;
        let age0 = max(age - trail, 0.0);
        let pos0 = em + vel * age0 + 0.5 * g * age0 * age0;
        let k = age / life;
        let r = sz * (1.0 - shrink * k);
        let dist = wh_seg_dist(p.xy, pos0, pos);
        let hue = clamp(abs(fract(hc + vec3<f32>(1.0, 0.6666667, 0.3333333)) * 6.0 - 3.0) - 1.0, vec3<f32>(0.0, 0.0, 0.0), vec3<f32>(1.0, 1.0, 1.0));
        let rgb = select(mix(ca.rgb, cb.rgb, hc), hue, rainbow);
        let a = select(mix(ca.a, cb.a, hc), 1.0, rainbow) * (1.0 - smoothstep(r * 0.5, r, dist)) * (1.0 - k);
        light = light + rgb * a;
        asum = asum + a;
    }
}
let alpha = clamp(asum, 0.0, 1.0);
return vec4<f32>(min(light / max(alpha, 0.000001), vec3<f32>(1.0, 1.0, 1.0)), alpha);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> sparkNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode(
        "wh.generator.lightning", "Lightning", "Generators", {fieldOutput()},
        {number("rate", "Strikes per second", 0.5, 0.01, 10.0), integer("bolts", "Bolts", 1, 1, 3),
         number("duration", "Duration (s)", 0.35, 0.02, 5.0), number("decay", "Decay", 1.2, 0.1, 10.0),
         number("width", "Width", 0.05, 0.002, 0.3), number("jag", "Jaggedness", 0.12, 0.0, 0.5),
         number("glow", "Glow", 0.4, 0.0, 1.0), number("branches", "Branches", 0.6, 0.0, 1.0),
         number("reach", "Reach", 1.0, 0.05, 1.0), number("flash", "Flash", 0.15, 0.0, 1.0),
         color("color", "Color", ColorRgba{0.75F, 0.85F, 1.0F, 1.0F})},
        kLightningBody, kSparkPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.particles", "Particles", "Generators", {fieldOutput()},
        {integer("count", "Particles", 16, 1, 48), number("lifetime", "Lifetime (s)", 1.5, 0.1, 10.0),
         number("speed", "Speed", 0.9, 0.0, 3.0), number("direction", "Direction (deg)", 90.0, -360.0, 360.0),
         number("spread", "Spread (turns)", 0.3, 0.0, 1.0), number("gravity", "Gravity", 0.8, -3.0, 3.0),
         number("ex", "Emitter X", 0.5, 0.0, 1.0), number("ey", "Emitter Y", 0.1, 0.0, 1.0),
         number("size", "Size", 0.045, 0.005, 0.3), number("trail", "Trail (s)", 0.12, 0.0, 1.0),
         number("shrink", "Shrink", 0.7, 0.0, 1.0), choice("palette", "Palette", {"Two colors", "Rainbow"}),
         color("colorA", "Color A", ColorRgba{1.0F, 0.7F, 0.2F, 1.0F}),
         color("colorB", "Color B", ColorRgba{1.0F, 0.2F, 0.05F, 1.0F})},
        kParticlesBody, kSparkPrelude));

    return nodes;
}

}  // namespace whitehole::nodes::detail
