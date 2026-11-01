// Volumen-Nodes: Effekte, die erst im Raum (x, y, z) Sinn ergeben, etwa für einen LED-Würfel.
//
// Warum eine eigene Datei: Die älteren Generatoren und Transformationen sind für die 2D-Leinwand entstanden
// und haben ihre z-Regler nachträglich bekommen (mit Standardwerten, die das 2D-Verhalten erhalten). Was
// hier steht, gab es vorher nicht: ein Quader, eine Schraube, ein Koordinaten-Testbild und ein Verdrehen
// des Raums um eine Achse. Auf einer 2D-Leinwand ist p.z = 0; diese Nodes laufen dort ohne Fehler, zeigen
// aber wenig. Alle sind zustandslos und rechnen nur aus Koordinate und Zeit.
//
// Regel für Preludes: nur gleiche Preludes werden zusammengelegt. Der Twist hat eine eigene Funktion mit
// eindeutigem Namen; Box und Helix nutzen das Farb-Prelude beziehungsweise mischen im Rumpf.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Quader: innen colorInside, außen colorOutside, dazwischen ein weicher Rand. Abstand zur Oberfläche eines
// achsenparallelen Quaders (Vorzeichen negativ innen); gemessen in Einheitswürfel-Koordinaten wie bei der Kugel.
constexpr const char* kBoxBody =
    R"wgsl(let c = vec3<f32>(@param(cx), @param(cy), @param(cz));
let ext = vec3<f32>(@param(sizeX), @param(sizeY), @param(sizeZ)) * 0.5;
let q = abs(p - c) - ext;
let d = length(max(q, vec3<f32>(0.0, 0.0, 0.0))) + min(max(q.x, max(q.y, q.z)), 0.0);
let t = smoothstep(0.0, max(@param(softness), 0.0001), d);
return wh_mix_color(@param(colorInside), @param(colorOutside), t);)wgsl";

// Schraube um eine senkrechte Achse durch (cx, cz): `strands` Linien winden sich mit `pitch` Umdrehungen
// pro Höhe 1 nach oben und drehen sich mit `speed` Umdrehungen pro Sekunde. Der Abstand zur Linie ist die
// Länge aus (Bogenlänge zum nächsten Strang, Radius - radius); `thickness` ist der Halbmesser der Linie.
constexpr const char* kHelixBody =
    R"wgsl(let d = p.xz - vec2<f32>(@param(cx), @param(cz));
let r = length(d);
let n = max(floor(@param(strands)), 1.0);
let turns = atan2(d.y, d.x) / 6.2831853 - @param(pitch) * p.y - @time * @param(speed);
let near = abs(fract(turns * n + 0.5) - 0.5);
let arc = near / n * 6.2831853 * r;
let dist = length(vec2<f32>(arc, r - @param(radius)));
let w = max(@param(thickness), 0.001);
let s = clamp(@param(softness), 0.01, 1.0);
let v = 1.0 - smoothstep(w * (1.0 - s), w, dist);
let col = @param(color);
return vec4<f32>(col.rgb, col.a * v);)wgsl";

// Testbild für die Ausrichtung: x wird rot, y grün, z blau. So sieht man am Würfel sofort, welche Seite
// wo liegt und ob Verkabelung und Startecke stimmen. `gain` hellt auf oder dunkelt ab.
constexpr const char* kPositionBody =
    R"wgsl(let g = max(@param(gain), 0.0);
return vec4<f32>(clamp(p.x * g, 0.0, 1.0), clamp(p.y * g, 0.0, 1.0), clamp(p.z * g, 0.0, 1.0), 1.0);)wgsl";

// Verdreht den Raum um eine Achse durch die Mitte (0.5, 0.5, 0.5): Je weiter eine Ebene von der Mitte
// entlang der Achse entfernt ist, desto stärker wird sie gedreht (`angle` Grad pro Höhe 1). Aus einer geraden
// Säule wird so eine Spirale. Gesampelt wird mit der Umkehrung; das Vorzeichen ändert nur die Drehrichtung.
constexpr const char* kTwistPrelude = R"wgsl(fn wh_twist_rotate(d: vec2<f32>, a: f32) -> vec2<f32> {
    return vec2<f32>(cos(a) * d.x - sin(a) * d.y, sin(a) * d.x + cos(a) * d.y);
}
)wgsl";

constexpr const char* kTwistBody =
    R"wgsl(let axis = i32(floor(@param(axis) + 0.5));
let a = radians(@param(angle));
var q = p;
if (axis == 0) {
    let r = wh_twist_rotate(p.yz - vec2<f32>(0.5, 0.5), a * (p.x - 0.5)) + vec2<f32>(0.5, 0.5);
    q = vec3<f32>(p.x, r.x, r.y);
} else if (axis == 1) {
    let r = wh_twist_rotate(p.xz - vec2<f32>(0.5, 0.5), a * (p.y - 0.5)) + vec2<f32>(0.5, 0.5);
    q = vec3<f32>(r.x, p.y, r.y);
} else {
    let r = wh_twist_rotate(p.xy - vec2<f32>(0.5, 0.5), a * (p.z - 0.5)) + vec2<f32>(0.5, 0.5);
    q = vec3<f32>(r.x, r.y, p.z);
}
return @in(source, q);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> volumeNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode(
        "wh.generator.box", "Box", "Generators", {fieldOutput()},
        {number("cx", "Center X", 0.5, 0.0, 1.0), number("cy", "Center Y", 0.5, 0.0, 1.0),
         number("cz", "Center Z", 0.5, 0.0, 1.0), number("sizeX", "Size X", 0.5, 0.0, 2.0),
         number("sizeY", "Size Y", 0.5, 0.0, 2.0), number("sizeZ", "Size Z", 0.5, 0.0, 2.0),
         number("softness", "Softness", 0.1, 0.0, 1.0),
         color("colorInside", "Inside", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
         color("colorOutside", "Outside", ColorRgba{0.0F, 0.0F, 0.0F, 1.0F})},
        kBoxBody, kColorPrelude));

    nodes.push_back(fieldNode(
        "wh.generator.helix", "Helix", "Generators", {fieldOutput()},
        {number("cx", "Center X", 0.5, 0.0, 1.0), number("cz", "Center Z", 0.5, 0.0, 1.0),
         number("radius", "Radius", 0.3, 0.0, 1.0), number("thickness", "Thickness", 0.06, 0.001, 0.5),
         number("softness", "Softness", 0.5, 0.0, 1.0), integer("strands", "Strands", 2, 1, 8),
         number("pitch", "Turns per height", 1.5, -8.0, 8.0), number("speed", "Rotation (turns/s)", 0.25, -10.0, 10.0),
         color("color", "Color", ColorRgba{0.2F, 0.8F, 1.0F, 1.0F})},
        kHelixBody));

    nodes.push_back(fieldNode("wh.generator.position", "Position Test", "Generators", {fieldOutput()},
                              {number("gain", "Gain", 1.0, 0.0, 4.0)}, kPositionBody));

    nodes.push_back(fieldNode("wh.transform.twist", "Twist", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {choice("axis", "Axis", {"X", "Y (vertical)", "Z"}, 1),
                               number("angle", "Angle (deg per unit)", 90.0, -1440.0, 1440.0)},
                              kTwistBody, kTwistPrelude));

    return nodes;
}

}  // namespace whitehole::nodes::detail
