// Expanding Shapes: Formen, die von einem Punkt nach außen laufen (fortlaufend) oder einmal auf Zuruf (One shot).
//
// Warum eine eigene Datei: Der Node hat seit 0.86.0 zehn Formen, vier Füllungen und zwei Laufarten; in
// builtin_shapes.cpp (Metaballs, Starfield, ...) wäre er der größte Block. Alles baut auf einem Abstand d(p) zum
// Mittelpunkt auf, der je nach Form anders gemessen wird (Kreis euklidisch, Quadrat als Maximum, Raute als Summe,
// Vielecke und Sterne über ihre Kanten). Aus d entsteht u, der Abstand zur nächsten Ringkante in Einheiten von
// `spacing`; die Füllung macht aus u eine Helligkeit. So sind Form, Füllung und Laufart unabhängig voneinander.
//
// Laufarten:
//   - Continuous: Ringe im Abstand `spacing`, die mit `speed` wandern (negativ: nach innen). Wie vor 0.86.0.
//   - One shot: genau eine Form, deren Größe der Eingang `progress` (0..1) bestimmt; bei 0 und 1 ist nichts zu sehen.
//     Den Fortschritt liefert der Shot-Node (Trigger → progress), denn Field-Nodes haben kein Gedächtnis für eine
//     Flanke. `inward` lässt die Form von außen zusammenfallen, `reach` ist die Größe bei progress = 1.
// Füllungen: Ring (Linie der Breite `thickness` × spacing), Solid (innen voll, fortlaufend: Streifen mit Tastgrad
// `thickness`), Gradient inward (an der Kante hell, nach innen ausklingend), Gradient outward (nach außen). `softness`
// ist die Weichheit der Kanten (0 hart, 1 wie bisher), `invert` kehrt die Füllung um (One shot + Solid: Blende).
// Mit space = Volume gelten die Abstände räumlich (Kugel-, Würfel-, Oktaederschalen; Vielecke als Prismen).
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kExpandingPrelude = R"wgsl(// Regelmäßiges n-Eck mit Umkreisradius 1: d = 1 auf den Ecken. `offset` dreht die erste Ecke hin (Dreieck: Spitze oben).
fn wh_exp_polygon(xy: vec2<f32>, n: f32, offset: f32) -> f32 {
    let sector = 6.2831853 / n;
    let a = atan2(xy.y, xy.x) - offset;
    let t = a - sector * floor(a / sector) - sector * 0.5;
    return length(xy) * cos(t) / cos(sector * 0.5);
}

// Stern mit n Zacken, Umkreisradius 1, innere Ecken bei `inner`: gerade Kanten zwischen äußerer und innerer Ecke.
fn wh_exp_star(xy: vec2<f32>, n: f32, inner: f32) -> f32 {
    let sector = 6.2831853 / n;
    let a = atan2(xy.y, xy.x) - 1.5707963;
    let t = sector * 0.5 - abs(a - sector * floor(a / sector) - sector * 0.5);  // 0 an der äußeren Ecke, sector/2 an der inneren
    let len = length(xy);
    let p = vec2<f32>(cos(t), sin(t)) * len;
    let outer = vec2<f32>(1.0, 0.0);
    let in_pt = vec2<f32>(cos(sector * 0.5), sin(sector * 0.5)) * inner;
    let edge = in_pt - outer;
    let normal = vec2<f32>(edge.y, -edge.x);
    return dot(p, normal) / dot(outer, normal);
}

fn wh_exp_dist(d0: vec3<f32>, shape: i32, points: f32) -> f32 {
    let ax = abs(d0);
    var d = length(d0);
    if (shape == 1) {
        d = max(max(ax.x, ax.y), ax.z);
    } else if (shape == 2) {
        d = ax.x + ax.y + ax.z;
    } else if (shape == 3) {
        d = max(wh_exp_polygon(d0.xy, 3.0, 1.5707963), ax.z);
    } else if (shape == 4) {
        d = max(wh_exp_polygon(d0.xy, 6.0, 0.0), ax.z);
    } else if (shape == 5) {
        d = max(wh_exp_polygon(d0.xy, points, 1.5707963), ax.z);
    } else if (shape == 6) {
        d = max(wh_exp_star(d0.xy, points, 0.45), ax.z);
    } else if (shape == 7) {
        // Kreuz aus drei Balken (Armbreite ein Drittel); in der Fläche ist ax.z = 0 und der z-Balken fällt weg.
        d = min(min(max(max(ax.x, ax.y * 3.0), ax.z * 3.0), max(max(ax.x * 3.0, ax.y), ax.z * 3.0)),
                max(max(ax.x * 3.0, ax.y * 3.0), ax.z));
    } else if (shape == 8) {
        d = ax.y;
    } else if (shape == 9) {
        d = ax.x;
    }
    return d;
}
)wgsl";

constexpr const char* kExpandingBody = R"wgsl(let center = vec3<f32>(@param(cx), @param(cy), @param(cz));
let volume = ((@param(space) > 0.5 && @param(space) < 1.5) || (@param(space) >= 1.5 && @volume));
var d0 = select(vec3<f32>(p.xy - center.xy, 0.0), p - center, volume);
let rot = radians(@param(rotation));
let cr = cos(rot);
let sr = sin(rot);
d0 = vec3<f32>(d0.x * cr - d0.y * sr, d0.x * sr + d0.y * cr, d0.z);
let shape = i32(floor(@param(shape) + 0.5));
let d = wh_exp_dist(d0, shape, clamp(floor(@param(points) + 0.5), 3.0, 12.0));
let spacing = max(@param(spacing), 0.02);
let w = clamp(@param(thickness), 0.02, 1.0);
let soft = clamp(@param(softness), 0.0, 1.0);
let oneshot = @param(run) > 0.5;
let progress = clamp(@param(progress), 0.0, 1.0);
// u: Abstand zur nächsten Ringkante in Einheiten von spacing, negativ innen (zur Mitte hin), positiv außen.
var u = 0.0;
if (oneshot) {
    let r = select(progress, 1.0 - progress, @param(inward) > 0.5) * max(@param(reach), 0.01);
    u = (d - r) / spacing;
} else {
    u = fract((d - @time * @param(speed)) / spacing) - 0.5;
}
let fill = i32(floor(@param(fill) + 0.5));
let half = w * 0.5;
let edge = max(soft * 0.1, 0.0001);  // Kantenbreite der Füllungen: bis zu einem Zehntel des Ringabstands
var v = 0.0;
if (fill == 1) {
    v = 1.0 - smoothstep(-edge, 0.0, u + select(0.0, 0.5 - w, !oneshot));
} else if (fill == 2) {
    v = clamp(1.0 + u / w, 0.0, 1.0) * (1.0 - smoothstep(0.0, edge, u));
} else if (fill == 3) {
    v = clamp(1.0 - u / w, 0.0, 1.0) * smoothstep(-edge, 0.0, u);
} else {
    v = 1.0 - smoothstep(half * (1.0 - soft), half, abs(u));
}
if (@param(invert) > 0.5) {
    v = 1.0 - v;
}
if (oneshot && (progress <= 0.0 || progress >= 1.0)) {
    v = 0.0;
}
v = v * clamp(1.0 - d * @param(fade), 0.0, 1.0);
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> expandingNodes() {
    std::vector<compiler::NodeDefinition> nodes;
    nodes.push_back(fieldNode(
        "wh.generator.shapes", "Expanding Shapes", "Generators", {fieldOutput()},
        {choice("shape", "Shape",
                {"Circle", "Square", "Diamond", "Triangle", "Hexagon", "Polygon (Points)", "Star (Points)", "Cross",
                 "Bars (horizontal)", "Bars (vertical)"}),
         integer("points", "Points (polygon, star)", 5, 3, 12), number("rotation", "Rotation (degrees)", 0.0, -180.0, 180.0),
         choice("fill", "Fill", {"Ring", "Solid", "Gradient inward", "Gradient outward"}),
         number("softness", "Edge softness", 1.0, 0.0, 1.0), toggle("invert", "Invert fill", false),
         choice("run", "Run", {"Continuous", "One shot (progress 0..1)"}),
         number("progress", "Progress (one shot)", 0.0, 0.0, 1.0), toggle("inward", "Inward (one shot)", false),
         number("reach", "Reach at progress 1", 1.0, 0.01, 3.0),
         number("cx", "Center X", 0.5, 0.0, 1.0), number("cy", "Center Y", 0.5, 0.0, 1.0),
         number("spacing", "Spacing", 0.3, 0.02, 2.0), number("speed", "Speed", 0.2, -5.0, 5.0),
         number("thickness", "Thickness", 0.3, 0.02, 1.0), number("fade", "Fade out", 0.5, 0.0, 4.0),
         color("color", "Color", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}), number("cz", "Center Z", 0.5, 0.0, 1.0),
         choice("space", "Space", {"Flat (x/y)", "Volume (x/y/z)", "Auto (by layout)"}, 2)},
        kExpandingBody, kExpandingPrelude));
    return nodes;
}

}  // namespace whitehole::nodes::detail
