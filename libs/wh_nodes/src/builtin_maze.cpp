// Labyrinth: leuchtende Läufer ziehen rechtwinklige Bahnen über ein Raster, mit hellem Kopf und verblassendem Schweif.
//
// Warum eine eigene Datei: Der Node ist wie Lightning und Particles ein Effekt mit Zufall und Bewegung, aber mit
// einer eigenen Idee (Wege auf einem Raster statt Wurfparabeln) und einem eigenen Prelude; builtin_sparks.cpp bliebe
// sonst zu lang.
//
// Zustandslos wie alle Field-Nodes: Jeder Läufer i hat einen Seed, aus dem sein Weg deterministisch folgt. Die Zeit
// wird in Schritten (Zellen) gezählt; alle E = 24 Schritte beginnt eine neue Epoche an einem gewürfelten Startpunkt.
// Innerhalb einer Epoche entscheidet an jeder Zelle ein Hash: geradeaus oder abbiegen; am Rand wird abgebogen oder
// umgekehrt. Der Shader läuft die aktuelle und die vorige Epoche Schritt für Schritt ab (die Position entsteht
// dabei nebenbei, darum kostet es nur O(E) je Läufer) und misst den Abstand des Pixels zu jedem Wegstück. Das Alter
// eines Punkts auf dem Weg (in Schritten hinter dem Kopf) bestimmt Helligkeit und Farbe; ab `trail` Schritten ist
// nichts mehr zu sehen. So kann kein Weg abreißen, obwohl es keine Liste von Frame zu Frame gibt: Dasselbe Bild kommt
// bei jedem Abspielen heraus.
//
// Raster: `cells` Zellen über die Breite; die Höhe bekommt `cells / aspect` Zellen (aspect = Breite/Höhe der
// Leinwand, 2 bei 32x16). Mit Space = Volume laufen die Läufer durch den Würfel (sechs Richtungen).
// Die Bahn eines Läufers liegt auf Zellmitten: Bei `cells` = Anzahl der LEDs über die Breite liegt jede Bahn genau
// auf einer LED-Reihe.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kMazePrelude = R"wgsl(fn wh_maze_hash(p: vec3<f32>) -> f32 {
    var q = fract(p * vec3<f32>(0.1031, 0.1030, 0.0973));
    q = q + dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

fn wh_maze_hsv(h: f32) -> vec3<f32> {
    let k = fract(vec3<f32>(h, h + 2.0 / 3.0, h + 1.0 / 3.0)) * 6.0;
    return clamp(abs(k - 3.0) - 1.0, vec3<f32>(0.0), vec3<f32>(1.0));
}

// Abbiegen: eine Richtung quer zur bisherigen (in der Fläche links/rechts, im Raum eine von vieren).
fn wh_maze_turn(dir: vec3<f32>, h: f32, volume: bool) -> vec3<f32> {
    let along_x = abs(dir.x) > 0.5;
    let along_y = abs(dir.y) > 0.5;
    let sign = select(-1.0, 1.0, fract(h * 7.0) > 0.5);
    if (!volume) {
        return select(vec3<f32>(sign, 0.0, 0.0), vec3<f32>(0.0, sign, 0.0), along_x);
    }
    let pick_first = h < 0.5;
    if (along_x) {
        return select(vec3<f32>(0.0, 0.0, sign), vec3<f32>(0.0, sign, 0.0), pick_first);
    }
    if (along_y) {
        return select(vec3<f32>(0.0, 0.0, sign), vec3<f32>(sign, 0.0, 0.0), pick_first);
    }
    return select(vec3<f32>(0.0, sign, 0.0), vec3<f32>(sign, 0.0, 0.0), pick_first);
}

fn wh_maze_inside(c: vec3<f32>, n: vec3<f32>) -> bool {
    return c.x >= 0.0 && c.y >= 0.0 && c.z >= 0.0 && c.x < n.x && c.y < n.y && c.z < n.z;
}

// Abstand von q zur Strecke a-b und Lage t des nächsten Punkts (0 = a, 1 = b).
fn wh_maze_seg(q: vec3<f32>, a: vec3<f32>, b: vec3<f32>) -> vec2<f32> {
    let ab = b - a;
    let t = clamp(dot(q - a, ab) / max(dot(ab, ab), 0.0000001), 0.0, 1.0);
    return vec2<f32>(length(q - (a + ab * t)), t);
}
)wgsl";

// Je Läufer zwei Epochen (vorige und aktuelle) zu je kEpoch Schritten; der Schweif ist höchstens kEpoch Schritte lang,
// darum reichen die beiden aus. Die Schleifen haben feste Grenzen (WGSL), `runners` und `trail` schalten innen.
constexpr const char* kMazeBody = R"wgsl(let volume = ((@param(space) > 0.5 && @param(space) < 1.5) || (@param(space) >= 1.5 && @volume));
let ncells = clamp(floor(@param(cells) + 0.5), 2.0, 64.0);
let aspect = max(@param(aspect), 0.1);
let n = vec3<f32>(ncells, max(floor(ncells / aspect + 0.5), 1.0), select(1.0, ncells, volume));
let cs = 1.0 / ncells;
let q = vec3<f32>(p.x, p.y / aspect, select(0.0, p.z, volume));
let runners = i32(clamp(floor(@param(runners) + 0.5), 1.0, 8.0));
let trail = clamp(@param(trail), 1.0, 24.0);
// Dicke und Leuchthof in Zellen: LED-Mitten liegen je nach Aufbau nicht genau auf Zellmitten (Position i/(n-1) statt
// (i+0.5)/n), darum deckt die Standarddicke 0,7 Zellen eine ganze LED-Reihe sicher ab.
let thick = max(@param(thickness), 0.01) * cs;
let glow = max(@param(glow), 0.01) * cs;
let head = max(@param(head), 0.0);
let turn = clamp(@param(turn), 0.0, 1.0);
let straight = clamp(floor(@param(straight) + 0.5), 1.0, 8.0);
let fade = max(@param(fade), 0.1);
let seed = @param(seed) * 17.0;
let palette = i32(floor(@param(palette) + 0.5));
let ca = @param(colorA);
let cb = @param(colorB);
var light = vec3<f32>(0.0, 0.0, 0.0);
var asum = 0.0;
// Hintergrund: schwaches Raster aus Hash-Strecken (je Zelle waagerecht oder senkrecht durch die Mitte).
if (@param(grid) > 0.0) {
    let cell = floor(q / cs);
    let f = fract(q / cs) - 0.5;
    let horizontal = wh_maze_hash(cell + vec3<f32>(seed + 3.0, 5.0, 7.0)) < 0.5;
    let dg = select(abs(f.x), abs(f.y), horizontal) * cs;
    let g = (1.0 - smoothstep(0.0, thick, dg)) * clamp(@param(grid), 0.0, 1.0) * cb.a;
    light = light + cb.rgb * g;
    asum = asum + g;
}
for (var i = 0; i < 8; i = i + 1) {
    if (i < runners) {
        let fi = f32(i);
        let rs = seed + fi * 13.0;
        let steps = @time * max(@param(speed), 0.0) + wh_maze_hash(vec3<f32>(rs, 1.0, 1.0)) * 24.0;
        let epoch = floor(steps / 24.0);
        let m = steps - epoch * 24.0;  // Schritte in der aktuellen Epoche (mit Bruchteil)
        var runner_col = ca.rgb;
        if (palette == 1) {
            runner_col = mix(ca.rgb, cb.rgb, wh_maze_hash(vec3<f32>(rs, 2.0, 2.0)));
        } else if (palette == 2) {
            runner_col = wh_maze_hsv(fi / f32(runners) + seed * 0.01);
        }
        for (var e = 0; e < 2; e = e + 1) {
            let ep = epoch - 1.0 + f32(e);
            let epk = ep - 997.0 * floor(ep / 997.0);
            let es = vec3<f32>(rs, epk * 3.0 + 11.0, 0.0);
            var cell = floor(vec3<f32>(wh_maze_hash(es + vec3<f32>(0.0, 0.0, 1.0)), wh_maze_hash(es + vec3<f32>(0.0, 0.0, 2.0)),
                                       wh_maze_hash(es + vec3<f32>(0.0, 0.0, 3.0))) * n);
            cell = min(cell, n - 1.0);
            var dir = wh_maze_turn(vec3<f32>(0.0, 0.0, 1.0), wh_maze_hash(es + vec3<f32>(0.0, 0.0, 4.0)), volume);
            if (!volume && abs(dir.z) > 0.5) {
                dir = vec3<f32>(1.0, 0.0, 0.0);
            }
            var since = straight;  // Schritte seit dem letzten Abbiegen
            for (var j = 0; j < 24; j = j + 1) {
                let fj = f32(j);
                if (e == 1 && fj > m) {
                    break;
                }
                let hj = wh_maze_hash(es + vec3<f32>(0.0, 0.0, 10.0 + fj));
                let turned = hj < turn && since >= straight;
                since = select(since + 1.0, 0.0, turned);
                if (turned) {
                    dir = wh_maze_turn(dir, wh_maze_hash(es + vec3<f32>(0.0, 0.0, 40.0 + fj)), volume);
                }
                var next = cell + dir;
                if (!wh_maze_inside(next, n)) {
                    // Am Rand: nach einem Abbiegen in die Wand lieber zur anderen Seite abbiegen (weg vom Rand), sonst
                    // quer abbiegen; als Letztes zurück. So kleben die Läufer nicht an den Rändern.
                    dir = select(wh_maze_turn(dir, wh_maze_hash(es + vec3<f32>(0.0, 0.0, 70.0 + fj)), volume), -dir, turned);
                    next = cell + dir;
                    if (!wh_maze_inside(next, n)) {
                        dir = select(-dir, wh_maze_turn(dir, 0.3, volume), turned);
                        next = cell + dir;
                        if (!wh_maze_inside(next, n)) {
                            dir = -dir;
                            next = cell + dir;
                        }
                    }
                }
                let a = (cell + 0.5) * cs;
                var b = (next + 0.5) * cs;
                // Alter des Streckenanfangs in Schritten hinter dem Kopf; e == 0 ist die vorige Epoche.
                let base_age = select(m + 24.0 - fj, m - fj, e == 1);
                var partial = 1.0;
                if (e == 1 && fj + 1.0 > m) {
                    partial = m - fj;
                    b = a + dir * cs * partial;
                }
                if (base_age - partial < trail + 1.0 && wh_maze_inside(next, n)) {
                    let sd = wh_maze_seg(q, a, b);
                    let age = max(base_age - sd.y * partial, 0.0);
                    let life = clamp(1.0 - age / trail, 0.0, 1.0);
                    let body = (1.0 - smoothstep(thick * 0.6, thick, sd.x)) * pow(life, fade);
                    var col = runner_col;
                    if (palette == 0) {
                        col = mix(ca.rgb, cb.rgb, 1.0 - life);
                    }
                    var a_px = body * mix(ca.a, cb.a, 1.0 - life);
                    if (e == 1 && fj + 1.0 > m) {
                        // Kopf: heller Punkt mit Leuchthof
                        let dh = length(q - b);
                        let halo = head * exp(-dh / glow) * (1.0 - smoothstep(0.0, glow * 4.0, dh));
                        col = mix(col, vec3<f32>(1.0, 1.0, 1.0), clamp(halo * 0.6, 0.0, 0.6));
                        a_px = clamp(a_px + halo * ca.a, 0.0, 1.0);
                    }
                    light = light + col * a_px;
                    asum = asum + a_px;
                }
                cell = next;
            }
        }
    }
}
let alpha = clamp(asum, 0.0, 1.0);
return vec4<f32>(min(light / max(asum, 0.000001), vec3<f32>(1.0, 1.0, 1.0)), alpha);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> mazeNodes() {
    std::vector<compiler::NodeDefinition> nodes;
    nodes.push_back(fieldNode(
        "wh.generator.maze", "Labyrinth", "Generators", {fieldOutput()},
        {integer("cells", "Cells across", 8, 2, 64), number("aspect", "Aspect (width / height)", 1.0, 0.1, 8.0),
         integer("runners", "Runners", 4, 1, 8), number("speed", "Speed (cells per second)", 3.0, 0.0, 30.0),
         number("trail", "Trail (cells)", 10.0, 1.0, 24.0), number("thickness", "Thickness (cells)", 0.7, 0.1, 4.0),
         number("glow", "Head glow (cells)", 1.0, 0.1, 8.0), number("head", "Head brightness", 1.0, 0.0, 3.0),
         number("turn", "Turn chance", 0.35, 0.0, 1.0),
         integer("straight", "Min. straight (cells)", 2, 1, 8), number("fade", "Fade (curve)", 1.0, 0.1, 4.0),
         choice("palette", "Palette", {"Along trail (A head, B tail)", "Per runner (mix of A and B)", "Rainbow"}),
         color("colorA", "Color A", ColorRgba{1.0F, 0.2F, 0.6F, 1.0F}),
         color("colorB", "Color B", ColorRgba{0.1F, 0.5F, 1.0F, 1.0F}),
         number("grid", "Background grid", 0.0, 0.0, 1.0), number("seed", "Seed", 0.0, 0.0, 100.0),
         choice("space", "Space", {"Flat (x/y)", "Volume (x/y/z)", "Auto (by layout)"}, 2)},
        kMazeBody, kMazePrelude));
    return nodes;
}

}  // namespace whitehole::nodes::detail
