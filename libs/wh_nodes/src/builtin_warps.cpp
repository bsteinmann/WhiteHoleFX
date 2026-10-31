// Verzerrungen in Polarkoordinaten: Kaleidoskop, Tunnel und Wirbel.
//
// Warum eine eigene Datei: builtin_transforms.cpp hat die einfachen Koordinatentricks (Drehen, Spiegeln,
// Kacheln ...). Diese drei rechnen um einen Mittelpunkt in Winkel und Abstand und brauchen alle denselben
// Hinweis zum Seitenverhältnis: Der Shader kennt die Leinwandgröße nicht, `aspect` (Breite / Höhe, 32x16 = 2)
// streckt x vor der Umrechnung, damit Kreise Kreise bleiben, und staucht zurück, bevor der Eingang abgetastet
// wird. Wie alle Transformationen ändern sie nur die Abtastkoordinate und sind zustandslos.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Kaleidoskop: Die Ebene wird um (cx, cy) in `segments` Keile geteilt; jeder zweite Keil ist das Spiegelbild
// seines Nachbarn, dazwischen liegt die Vorlage. `angle` dreht das Ausschnittsfenster, `zoom` vergrößert.
// Mit n Keilen und einem Winkel a ist a' = |mod(a, w) - w/2| mit w = 2π/n der gefaltete Winkel.
constexpr const char* kKaleidoscopeBody = R"wgsl(let asp = max(@param(aspect), 0.1);
let c = vec2<f32>(@param(cx), @param(cy));
let d = (p.xy - c) * vec2<f32>(asp, 1.0);
let r = length(d) / max(@param(zoom), 0.01);
let n = max(floor(@param(segments)), 1.0);
let w = 6.2831853 / n;
var a = atan2(d.y, d.x) + radians(@param(angle));
a = a - w * floor(a / w);
a = abs(a - w * 0.5);
let q = c + vec2<f32>(cos(a) * r / asp, sin(a) * r);
return @in(source, vec3<f32>(q.x, q.y, p.z));)wgsl";

// Tunnel: Winkel und Tiefe (1 / Abstand) werden zu den Koordinaten des Eingangs. Der Eingang erscheint
// `repeat`-mal um den Tunnel herum und läuft mit `speed` Kacheln pro Sekunde in die Tiefe (negativ: heraus).
// `twist` dreht den Tunnel mit der Tiefe, `depth` bestimmt, wie schnell die Kacheln zur Mitte hin kleiner
// werden, `fade` blendet die Mitte aus (Anteil des Radius, 0 = nicht).
constexpr const char* kTunnelBody = R"wgsl(let asp = max(@param(aspect), 0.1);
let c = vec2<f32>(@param(cx), @param(cy));
let d = (p.xy - c) * vec2<f32>(asp, 1.0);
let r = max(length(d), 0.0001);
let depth = @param(depth) / r + @time * @param(speed);
let u = atan2(d.y, d.x) / 6.2831853 * floor(max(@param(repeat), 1.0)) + depth * @param(twist);
let src = @in(source, vec3<f32>(fract(u), fract(depth), p.z));
let f = select(1.0, smoothstep(0.0, @param(fade), r), @param(fade) > 0.0001);
return vec4<f32>(src.rgb, src.a * f);)wgsl";

// Wirbel: Die Koordinate wird um (cx, cy) gedreht, und zwar umso stärker, je näher sie der Mitte liegt:
// im Mittelpunkt `turns` volle Umdrehungen, bei `radius` und außerhalb keine. Die Kurve (1 - r/radius)^2
// macht den Rand sanft.
constexpr const char* kVortexBody = R"wgsl(let asp = max(@param(aspect), 0.1);
let c = vec2<f32>(@param(cx), @param(cy));
let d = (p.xy - c) * vec2<f32>(asp, 1.0);
let r = length(d);
let k = clamp(1.0 - r / max(@param(radius), 0.001), 0.0, 1.0);
let a = -6.2831853 * @param(turns) * k * k;
let q = c + vec2<f32>((cos(a) * d.x - sin(a) * d.y) / asp, sin(a) * d.x + cos(a) * d.y);
return @in(source, vec3<f32>(q.x, q.y, p.z));)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> warpNodes() {
    std::vector<compiler::NodeDefinition> nodes;
    const auto centerAndAspect = [](std::vector<graph::ParameterDescriptor> own) {
        own.push_back(number("cx", "Center X", 0.5, 0.0, 1.0));
        own.push_back(number("cy", "Center Y", 0.5, 0.0, 1.0));
        own.push_back(number("aspect", "Canvas aspect (w/h)", 1.0, 0.1, 20.0));
        return own;
    };

    nodes.push_back(fieldNode("wh.transform.kaleidoscope", "Kaleidoscope", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              centerAndAspect({integer("segments", "Segments", 6, 1, 32), number("angle", "Angle (deg)", 0.0),
                                               number("zoom", "Zoom", 1.0, 0.1, 8.0)}),
                              kKaleidoscopeBody));

    nodes.push_back(fieldNode("wh.transform.tunnel", "Tunnel", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              centerAndAspect({number("speed", "Speed (tiles/s)", 0.3, -10.0, 10.0),
                                               integer("repeat", "Repeats around", 2, 1, 16),
                                               number("depth", "Depth", 0.25, 0.01, 4.0), number("twist", "Twist", 0.0, -4.0, 4.0),
                                               number("fade", "Fade center", 0.1, 0.0, 1.0)}),
                              kTunnelBody));

    nodes.push_back(fieldNode("wh.transform.vortex", "Vortex", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              centerAndAspect({number("turns", "Turns at center", 1.0, -8.0, 8.0),
                                               number("radius", "Radius", 0.5, 0.01, 2.0)}),
                              kVortexBody));
    return nodes;
}

}  // namespace whitehole::nodes::detail
