// Transformationen: verändern die Koordinate und werten den Eingang dort aus.
// Sie wirken dadurch auf jeden Effekt dahinter, ohne Zwischenbild.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Dreht die Abtastkoordinate um den Mittelpunkt (0.5, 0.5) um -angle. Das Bild
// selbst dreht sich damit um +angle: gegen den Uhrzeigersinn, denn y zeigt im ganzen
// Projekt nach oben (siehe wh_mapping/fixture.hpp).
constexpr const char* kRotateBody = R"wgsl(let a = radians(@param(angle));
let c = cos(a);
let s = sin(a);
let d = p.xy - vec2<f32>(0.5, 0.5);
let r = vec2<f32>(c * d.x + s * d.y, -s * d.x + c * d.y) + vec2<f32>(0.5, 0.5);
return @in(source, vec3<f32>(r.x, r.y, p.z));)wgsl";

// Spiegelt an einer Linie (x, y oder z, jeweils an einer einstellbaren Stelle): Die Koordinate wird
// auf die eine Seite gefaltet, die andere zeigt das Spiegelbild. Standard: die untere Hälfte (x < 0.5)
// ist die Vorlage; "Other half" nimmt stattdessen die obere.
constexpr const char* kMirrorBody = R"wgsl(var q = p;
let high = @param(otherHalf) > 0.5;
if (@param(mirrorX) > 0.5) {
    let c = @param(centerX);
    let d = abs(q.x - c);
    q.x = select(c - d, c + d, high);
}
if (@param(mirrorY) > 0.5) {
    let c = @param(centerY);
    let d = abs(q.y - c);
    q.y = select(c - d, c + d, high);
}
if (@param(mirrorZ) > 0.5) {
    let c = @param(centerZ);
    let d = abs(q.z - c);
    q.z = select(c - d, c + d, high);
}
return @in(source, q);)wgsl";

// "Region" (Bild-in-Bild): Der Eingang wird auf ein Rechteck der Leinwand abgebildet, als wäre dieses
// Rechteck die ganze Leinwand; außerhalb ist das Ergebnis durchsichtig (Alpha 0, die LED bleibt aus).
// left und top zählen von der linken oberen Ecke, alle vier Werte sind Anteile der Leinwand (0..1).
// Breite und Höhe haben eine Untergrenze, damit nie durch null geteilt wird. Die z-Koordinate bleibt.
constexpr const char* kRegionBody = R"wgsl(let w = max(@param(width), 0.0001);
let h = max(@param(height), 0.0001);
let u = (p.x - @param(left)) / w;
let v = (p.y - (1.0 - @param(top) - h)) / h;
if (u < -0.0001 || u > 1.0001 || v < -0.0001 || v > 1.0001) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
}
return @in(source, vec3<f32>(u, v, p.z));)wgsl";

// Zoom um die Mitte in x und y: factor 2 zeigt das Bild doppelt so groß.
constexpr const char* kScaleBody = R"wgsl(let c = vec3<f32>(0.5, 0.5, p.z);
return @in(source, (p - c) / @param(factor) + c);)wgsl";

// Dreht das Bild im Raum um die Mitte (0.5, 0.5, 0.5). Positive Winkel drehen rechtshändig
// um die jeweilige Achse (Daumen zeigt in Achsenrichtung): +x wandert unter angleZ nach +y,
// +y unter angleX nach +z, +z unter angleY nach +x. Die Reihenfolge ist R = Rz * Ry * Rx
// (erst um x, dann um y, dann um z). Gesampelt wird mit der Umkehrung R^-1.
constexpr const char* kRotate3dBody =
    R"wgsl(let c = vec3<f32>(0.5, 0.5, 0.5);
var d = p - c;
let az = radians(@param(angleZ));
d = vec3<f32>(cos(az) * d.x + sin(az) * d.y, -sin(az) * d.x + cos(az) * d.y, d.z);
let ay = radians(@param(angleY));
d = vec3<f32>(cos(ay) * d.x - sin(ay) * d.z, d.y, sin(ay) * d.x + cos(ay) * d.z);
let ax = radians(@param(angleX));
d = vec3<f32>(d.x, cos(ax) * d.y + sin(ax) * d.z, -sin(ax) * d.y + cos(ax) * d.z);
return @in(source, d + c);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> transformNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode("wh.transform.rotate", "Rotate", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("angle", "Angle (deg)", 0.0)}, kRotateBody));

    nodes.push_back(fieldNode("wh.transform.mirror", "Mirror", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {toggle("mirrorX", "Mirror X", true), toggle("mirrorY", "Mirror Y", false),
                               toggle("mirrorZ", "Mirror Z", false), number("centerX", "Center X", 0.5, 0.0, 1.0),
                               number("centerY", "Center Y", 0.5, 0.0, 1.0), number("centerZ", "Center Z", 0.5, 0.0, 1.0),
                               toggle("otherHalf", "Use other half", false)},
                              kMirrorBody));

    nodes.push_back(fieldNode("wh.transform.region", "Region", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("left", "Left", 0.0, 0.0, 1.0), number("top", "Top", 0.0, 0.0, 1.0),
                               number("width", "Width", 0.5, 0.0, 1.0), number("height", "Height", 0.5, 0.0, 1.0)},
                              kRegionBody));

    nodes.push_back(fieldNode("wh.transform.scale", "Scale", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("factor", "Factor", 1.0, 0.01, 100.0)}, kScaleBody));

    nodes.push_back(fieldNode("wh.transform.rotate3d", "Rotate 3D", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("angleX", "Angle X (deg)", 0.0), number("angleY", "Angle Y (deg)", 0.0),
                               number("angleZ", "Angle Z (deg)", 0.0)},
                              kRotate3dBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
